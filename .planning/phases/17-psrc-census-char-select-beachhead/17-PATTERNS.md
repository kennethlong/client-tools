# Phase 17: PSRC Census + Char-Select Beachhead - Pattern Map

**Mapped:** 2026-05-27
**Files analyzed:** 6 modified source files + 2 new artifacts (census log, COMPARISON dir)
**Analogs found:** 6 / 6 (all in-tree; this phase wires existing Phase 11 infra)

> This is a C++ engine/renderer phase. There are NO new files of significance — the work
> EXTENDS six existing files. "Closest analog" here means "the existing code in (or adjacent
> to) the same file that the new code must mirror byte-for-byte or structurally." Every excerpt
> below is VERIFIED from the live tree this session at the cited line.
>
> **Two load-bearing planning insights (from RESEARCH.md, re-confirmed):**
> 1. The census gate flag MUST use the SHARED `ConfigClientGraphics` / `ConfigFile::getKeyBool("ClientGraphics", ...)` — NOT the plugin's `ConfigDirect3d11` (`clientGraphics` cannot depend on the D3D11 plugin).
> 2. D3DReflect currently exists ONLY on the VS path (`Direct3d11_VertexShaderData.cpp:586-677`). That VS reflection loop is the EXACT pattern to mirror for PS cbuffer reflection — there is no PS-reflection precedent to copy, so copy the VS one.

## File Classification

| Modified/New File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `clientGraphics/.../ShaderImplementation.cpp` (`load_0000`, `:2895-2911`) | shared asset loader (IFF) | file-I/O / transform | self — `m_exe`/`TAG_PEXE` read in same method + VS `m_text` retain `:2305-2319` | exact (in-file sibling) |
| `clientGraphics/.../ShaderImplementation.h` (PS struct `:677-682`) | shared data struct | (storage) | VS struct `m_text`/`m_textLength` `:426-428` | exact (sibling struct) |
| `Direct3d11_PixelShaderProgramData.cpp` (ctor `:716-756`) | plugin shader factory | transform (HLSL→DXBC) | `compilePixelShaderFromHlsl` `:86-261` (already present, unused for assets) | exact (same file) |
| `Direct3d11_StaticShaderData.cpp` (`apply` deferral `:601-605`) | plugin per-draw constant upload | request-response (draw-time) | VS reflection `Direct3d11_VertexShaderData.cpp:586-677` + `setPixelShaderUserConstants_impl` `Direct3d11.cpp:660-694` | role-match (cross-file) |
| `Direct3d11_ConstantBuffer.h/.cpp` (`PerMaterialCB` `:58-66`, `updatePS` `:174-191`) | plugin cbuffer | transform (Map/upload) | self — fields declared, `updatePS` working | exact (already wired, fields zero) |
| census instrumentation (in `ShaderImplementation.cpp` + `ConfigClientGraphics`) | shared diagnostic logging | event-driven (per-load) | `ConfigClientGraphics` `getKeyBool` gate `:66,318` + classification `PixelShaderProgramView.cpp:304-313` + `AddApplicationMessage` emission `Direct3d11_PixelShaderProgramData.cpp:523-541` | role-match (composed from 3 analogs) |
| census log artifact + `docs/research/<char-select>/COMPARISON.md` | artifact | (output) | `docs/research/phase12-baseline/COMPARISON.md` | exact (mirror convention) |

---

## Pattern Assignments

### 1. `ShaderImplementation.cpp` — PSRC-retain edit (SHARED, the one dual-renderer-gated touch)

**Role:** shared asset loader · **Data flow:** file-I/O
**Analog:** the `TAG_PEXE`→`m_exe` read in the SAME method, plus the VS `m_text` retain precedent.

**The discard site to edit** (`ShaderImplementation.cpp:2895-2911`, VERIFIED):
```cpp
void ShaderImplementationPassPixelShaderProgram::load_0000(Iff &iff)
{
    iff.enterForm(TAG_0000);

        // load the source code
        iff.enterChunk(TAG_PSRC);
        iff.exitChunk(TAG_PSRC, true);   // <-- TODAY: source DROPPED. Retain edit reads it here.

        // load the d3d8 executable data
        iff.enterChunk(TAG_PEXE);
            const int exeLength = iff.getChunkLengthLeft(sizeof(DWORD));
            m_exe = new DWORD[exeLength];
            iff.read_uint32(exeLength, m_exe);
        iff.exitChunk(TAG_PEXE);

    iff.exitForm(TAG_0000);
}
```

**Copy-from pattern (the PEXE read is the template for the PSRC read):** allocate from
`iff.getChunkLengthLeft(...)`, read into a `new[]` buffer, exit the chunk. For PSRC the chunk
is a STRING (ShaderBuilder wrote it via `insertChunkString`; classification reads it as text at
`PixelShaderProgramView.cpp:304`), so read bytes + null-terminate (mirror VS `m_text` below) rather
than `read_uint32`.

**VS `m_text` retain precedent** (`ShaderImplementation.cpp:2305-2319`, VERIFIED — different
mechanism, same ownership shape):
```cpp
void ShaderImplementationPassPixelShaderVertexShader::load(CrcString const &fileName)
{
    AbstractFile *file = TreeFile::open(fileName.getString(), AbstractFile::PriorityData, false);
    m_textLength = file->length();
    m_text = new char[m_textLength+1];
    file->read(m_text, m_textLength);
    m_text[m_textLength] = '\0';      // <-- null-terminate; the PS retain must do the same
    delete file;
    ...
}
```
> NOTE the mechanism DIFFERS: VS opens by FILENAME via `TreeFile::open`; PS reads from the IFF
> chunk (`getChunkLengthLeft` + read). Copy the *ownership shape* (`new char[len+1]`, null-terminate,
> store length), not the `TreeFile::open` call.

**Destructor ownership** — free the new member exactly like `m_exe`
(`ShaderImplementation.cpp:2775-2785`, VERIFIED):
```cpp
ShaderImplementationPassPixelShaderProgram::~ShaderImplementationPassPixelShaderProgram()
{
    ...
    delete [] m_exe;          // <-- add `delete [] m_psrcText;` alongside
    delete m_graphicsData;
}
```

**CRITICAL constraints (D-06 / Pitfall 6):**
- Keep `load_0000` byte-for-byte BEHAVIORALLY identical for D3D9 except STORING the text. The
  `TAG_PEXE`→`m_exe` block must not change. D3D9 (`Direct3d9_PixelShaderProgramData.cpp:34`)
  keeps consuming `m_exe` unchanged.
- Boot-test BOTH `rasterMajor=5` AND `=11` on this edit (it is shared `clientGraphics`).
- Free the new member in the dtor; watch reload (`:2818`) ownership too. Leak at shutdown = regression.

---

### 2. `ShaderImplementation.h` — PS struct member add (SHARED)

**Role:** shared data struct · **Analog:** the VS struct's `m_text`/`m_textLength` public members.

**VS struct (the template)** (`ShaderImplementation.h:426-429`, VERIFIED):
```cpp
public:
    PersistentCrcString                                m_fileName;
    char                                              *m_text;        // <-- mirror these two
    int                                                m_textLength;  //     onto the PS struct
    ShaderImplementationPassVertexShaderGraphicsData  *m_graphicsData;
```

**PS struct to extend** (`ShaderImplementation.h:677-682`, VERIFIED — currently only `m_exe`):
```cpp
public:
    PersistentCrcString                                      m_fileName;
    DWORD                                                   *m_exe;
    // ADD HERE (mirror VS): char *m_psrcText; int m_psrcLen;  (public so the D3D11 plugin ctor reads them)
    ShaderImplementationPassPixelShaderProgramGraphicsData  *m_graphicsData;
```
> The members are PUBLIC because the D3D11 plugin ctor (`Direct3d11_PixelShaderProgramData`)
> reads them directly — same access pattern the ctor already uses for `pixelShaderProgram.m_exe`
> (`Direct3d11_PixelShaderProgramData.cpp:724`).

---

### 3. `Direct3d11_PixelShaderProgramData.cpp` — recompile lane wiring (PLUGIN, primary lane D-09)

**Role:** plugin shader factory · **Data flow:** transform (HLSL→DXBC→`ID3D11PixelShader`)
**Analog:** `compilePixelShaderFromHlsl` (SAME FILE, `:86-261`) — already present, already does
rewrite + `D3DCompile(ps_4_0)` + `.cso` cache + `CreatePixelShader`. The new work FEEDS PSRC text
into it from the ctor.

**The helper signature to call** (`Direct3d11_PixelShaderProgramData.cpp:86-90`, VERIFIED):
```cpp
bool compilePixelShaderFromHlsl(
    char const *sourceText,
    size_t sourceLen,
    char const *displayName,
    Microsoft::WRL::ComPtr<ID3D11PixelShader> &outComPtr);   // returns true + binds on success
```

**It already does everything** — defines (incl. `D3D11_PROFILE`, `D3D11_REWRITE_VERSION="21"` in the live tree — Round-5 review item 5: Plan 17-02 already bumped 20->21, verified :153 + :303; a future generator/rewrite change must bump to "22"),
cache lookup, `HlslRewrite::applyToMainSource`, `D3DCompile` with the proven flag set
(`ENABLE_BACKWARDS_COMPATIBILITY` + `PACK_MATRIX_ROW_MAJOR`, STRICTNESS dropped),
`ShaderCache::store`, then `CreatePixelShader` into `outComPtr` (`:147-260`, VERIFIED). Do NOT
re-implement any of it.

> CACHE-INVALIDATION RULE (Runtime State Inventory): if you change the rewrite rules or the PS
> compile generator, BUMP `D3D11_REWRITE_VERSION` (`:153` + `:303`, currently `"21"` in the live tree per Round-5 item 5 — Plan 17-02 bumped 20->21; next bump is "22") so stale `.cso`
> caches invalidate. Otherwise a stale cache masquerades as a success.

**The ctor to wire** (`Direct3d11_PixelShaderProgramData.cpp:716-756`, VERIFIED — currently
rejects PEXE and leaves `m_d3dPS` NULL):
```cpp
Direct3d11_PixelShaderProgramData::Direct3d11_PixelShaderProgramData(ShaderImplementationPassPixelShaderProgram const &pixelShaderProgram)
:   ShaderImplementationPassPixelShaderProgramGraphicsData(),
    m_program(&pixelShaderProgram),
    m_d3dPS()
{
    DWORD const *exe = pixelShaderProgram.m_exe;
    ...
    // Current pre-compiled D3D9 bytecode case. We CANNOT pass this to
    // CreatePixelShader -- it would E_INVALIDARG. Log the version ...
    DEBUG_REPORT_LOG_PRINT(true, ("... is pre-compiled D3D9 PS %d.%d bytecode ... m_d3dPS NULL ...", ...));
}
```

**Wiring shape (recompile-first, D-09):** before falling through to the PEXE-reject path, if
`pixelShaderProgram.m_psrcText` starts with `"//hlsl "` (classification — see §6), call
`compilePixelShaderFromHlsl(m_psrcText, m_psrcLen, getFileName(), m_d3dPS)` and `return` on
success. On compile FAILURE leave `m_d3dPS` NULL → existing magenta tombstone (KEEP it — anti-pattern
to remove). asm PSRC → secondary lane (port asm→HLSL→`ps_4_0`; NEVER re-assemble asm — named landmine).

**Member to bind** (`Direct3d11_PixelShaderProgramData.h:129`, VERIFIED):
```cpp
Microsoft::WRL::ComPtr<ID3D11PixelShader> m_d3dPS;   // null today; the recompile lane fills it
```

---

### 4. `Direct3d11_StaticShaderData.cpp` — D3DReflect-driven PS constant upload (PLUGIN, D-04)

**Role:** plugin per-draw constant upload · **Data flow:** request-response (draw-time `apply`)
**Analog A (reflection):** VS reflection loop `Direct3d11_VertexShaderData.cpp:586-677`.
**Analog B (upload mechanics):** `setPixelShaderUserConstants_impl` `Direct3d11.cpp:660-694`.

**The deferral site to fill** (`Direct3d11_StaticShaderData.cpp:601-606`, VERIFIED — the
"Still deferred to Phase 12" comment is exactly this work):
```cpp
// Still deferred to Phase 12 (real per-asset PS compile): material /
// textureFactor / textureScroll / alpha-test / stencil / fog / multi-slot
// SRV/sampler binding (slots 1..7). CODEX Q5 -- the dynamic fallback PS
// reads only t.Sample(s, input._v0.xy) ...
bool const cacheHit = (ms_active == this && ms_activePass == passNumber);
```
> D-04 scope: wire ONLY material color + textureFactor now. Defer textureScroll/fog/stencil to
> Phases 19/21. The reflection plumbing is generic so later phases ADD feeds, not re-architect.

**Analog A — the VS reflection loop to MIRROR for PS cbuffers**
(`Direct3d11_VertexShaderData.cpp:611-676`, VERIFIED — this is the ONLY D3DReflect precedent in the tree):
```cpp
ComPtr<ID3D11ShaderReflection> reflector;
HRESULT const reflectHr = D3DReflect(
    blob->GetBufferPointer(),
    blob->GetBufferSize(),
    IID_PPV_ARGS(reflector.GetAddressOf()));
if (SUCCEEDED(reflectHr) && reflector)
{
    D3D11_SHADER_DESC shaderDesc = {};
    if (SUCCEEDED(reflector->GetDesc(&shaderDesc)))
    {
        // VS walks InputParameters / OutputParameters here.
        // PS extension: walk reflector->GetConstantBufferByName("PerMaterial")
        //   -> GetVariableByName(...) -> GetDesc(&vd) to map material/textureFactor BY NAME,
        // and reflector->GetResourceBindingDesc(...) for SRV/sampler slot mapping.
        ...
    }
}
// Reflection failure is treated as defensive NON-FATAL (VS leaves vectors empty;
// PS should leave constants zero → no crash). MIRROR that non-fatal stance.
```

**Analog B — the upload mechanics + the slot-2 / `PerMaterialCB` target**
(`Direct3d11.cpp:660-694`, VERIFIED — this is the existing flush whose material fields are zero):
```cpp
static Direct3d11_PerMaterialCB s_perMaterialShadow = {};
... // populates only userConstants[] today; materialDiffuse/Specular/Emissive stay ZERO
// Plan 11-09.15 Iter-45: flush the PerMaterialCB shadow to PS slot 2.
// PS slot 2 is the PerMaterialCB binding (Direct3d11_HlslRewrite.cpp:797).
// "Material diffuse/specular/emissive remain zero here (Pass::apply uploads none -- Phase 12 scope)"
Direct3d11_ConstantBuffer::updatePS(2, &s_perMaterialShadow, sizeof(s_perMaterialShadow));
```
> THIS COMMENT IS THE PHASE 17 GAP. The zero-filled `materialDiffuse` etc. are exactly what the
> reflection-driven upload must populate. Phase 17 = "Phase 12 scope" in this comment.

**Anti-patterns (HARD RULES — VERIFIED in this file):**
- **D3D9 register folklore:** do NOT copy c11/c44/c45/c47. The recompiler reassigns; drive from
  D3DReflect by NAME. (Ties to id=342/343.)
- **Never call `PSSet*` directly from StaticShaderData** (CODEX Q1, `:705-709`): use
  `Direct3d11_ConstantBuffer::updatePS(slot,...)` / StateCache setters; preserve the lazy flush.
- **Do NOT re-enable per-pass blend factors** — Iter-44C REVERTED (`:686-702`); amplification
  regression until the real PS samples all SRVs.
- **Do NOT re-fold the depth/"eyes through head" fix** — Iter-44A already shipped (`:665-677`);
  verify by A/B, don't re-implement (D-07).
- **cbuffer matrices need `XMMatrixTranspose`** at upload (memory). Material color/textureFactor are
  float4 vectors (no transpose needed); if a texture-transform matrix is later added, transpose it.
- **Full-struct writes only:** `updatePS` uses `Map(WRITE_DISCARD)` (`:185`) — partial writes leave garbage.

---

### 5. `Direct3d11_ConstantBuffer.h/.cpp` — `PerMaterialCB` populate (PLUGIN, already wired)

**Role:** plugin cbuffer · **Analog:** self — struct declared, `updatePS` working, fields zero.

**The struct whose fields the reflection upload fills** (`Direct3d11_ConstantBuffer.h:58-66`, VERIFIED):
```cpp
struct Direct3d11_PerMaterialCB
{
    DirectX::XMFLOAT4   materialDiffuse;    // <-- D-04 populates (currently zero)
    DirectX::XMFLOAT4   materialSpecular;
    DirectX::XMFLOAT4   materialEmissive;
    DirectX::XMFLOAT4   userConstants[4];   // PS-side D3D9 PSCR_userConstant analog (already flowing)
};
static_assert(sizeof(Direct3d11_PerMaterialCB) == 112, "...");   // keep size/16-align if you add fields
```

**The upload helper (no change needed; just call it)** (`Direct3d11_ConstantBuffer.cpp:174-191`, VERIFIED):
```cpp
void Direct3d11_ConstantBuffer::updatePS(int slot, void const *data, size_t sizeBytes)
{
    // range-checks slot/size, Map(WRITE_DISCARD), memcpy full struct, Unmap.
    // textureFactor: if it needs its own field, add to PerMaterialCB (keep 16-byte align +
    // update the static_assert) OR reuse a userConstants[] slot per the reflected layout.
}
```

---

### 6. Census instrumentation (SHARED `clientGraphics` + `ConfigClientGraphics` gate) — D-01/D-03 gating deliverable

**Role:** shared diagnostic logging · **Data flow:** event-driven (per PS-program load)
Composed from THREE analogs (no single precedent exists):

**Analog A — the shared config gate (USE THIS, not `ConfigDirect3d11`).**
`ConfigClientGraphics` is shared `clientGraphics` and already gates load behavior via
`ConfigFile::getKeyBool("ClientGraphics", ...)` (`ConfigClientGraphics.cpp:66`, VERIFIED):
```cpp
#define KEY_BOOL(a,b)    (ms_ ## a = ConfigFile::getKeyBool("ClientGraphics", #a, b))
...
KEY_BOOL(logBadCustomizationData,             false);   // :101 — the shape to copy for a census flag
```
And a paired getter (`ConfigClientGraphics.cpp:318` style):
```cpp
bool ConfigClientGraphics::getLoadAllAssetsRegardlessOfShaderCapability() { return ms_...; }
```
> Add e.g. `KEY_BOOL(psrcCensus, false)` + a `getPsrcCensus()` getter, then gate the census
> emission in `load_0000` on it. This keeps the D3D9 path UNTOUCHED when off (D-01 / D-03 / the
> "census flag must use shared ConfigFile not ConfigDirect3d11" insight). The plugin's
> `ConfigDirect3d11` (`ConfigDirect3d11.cpp:34-35`) uses the SAME `ConfigFile` API but the
> `[Direct3d11]` section and is NOT linkable from `clientGraphics` — do not use it.

**Analog B — `//hlsl` vs asm classification (literal first-line prefix check).**
(`PixelShaderProgramView.cpp:304-313`, VERIFIED):
```cpp
CString left7 = line.Left(7);
left7.MakeLower();
if (left7 == "//hlsl ")               // <-- HLSL; profile = token after the prefix
{
    target = line.Mid(7);
    target.TrimLeft(" \t");
    target.TrimRight(" \t");
}
// else -> D3DXAssembleShader (asm)   [:316-328]
```
> Census rule + the recompile-lane branch (§3) use the SAME check: trim leading whitespace,
> lowercase the first 7 chars, `== "//hlsl "` → HLSL with profile = the trailing token; else asm.
> Re-implement this in the engine on the retained `m_psrcText` (the engine has no `CString` IFF
> view; do the prefix compare on the raw `char*`).

**Analog C — the observable log channel (`d3d11-debug.log` via InfoQueue).**
(`Direct3d11_PixelShaderProgramData.cpp:523-541`, VERIFIED — `DEBUG_REPORT_LOG_PRINT` is invisible
under explorer launch; `AddApplicationMessage` lands in `stage/d3d11-debug.log`):
```cpp
if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
{
    iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, header);
    iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, hlsl.c_str());
}
DEBUG_REPORT_LOG_PRINT(true, ("%s\n%s\n", header, hlsl.c_str()));
```
> CAVEAT: the InfoQueue lives in the D3D11 plugin (`Direct3d11_Device::getInfoQueue`), which
> shared `clientGraphics` CANNOT reach. Census emission from `load_0000` (shared) must use
> `DEBUG_REPORT_LOG_PRINT` (works) or a dedicated census file/stream opened in shared code.
> The InfoQueue `AddApplicationMessage` pattern is the analog for FORMAT (one record per shader:
> name, isHlsl, profile, includeCount, sampler slots, referenced constants) — copy the per-record
> shape, not the InfoQueue sink. Aggregate HLSL:asm ratio at end-of-boot as the phase artifact (D-03).

---

### 7. COMPARISON dir + naming convention (NEW artifact, D-05)

**Analog:** `docs/research/phase12-baseline/COMPARISON.md` (VERIFIED).
**Naming convention to mirror** (from its image inventory table):
```
{spot/anchor}_{renderer}_{shot}.jpg     e.g.  spotB_exterior_d3d11_0013.jpg
                                                spotB_exterior_d3d9_0011.jpg
```
For Phase 17 char-select: `{anchor}_{renderer}_{shot}.jpg`, e.g. `sul_eye_d3d11_NNNN.jpg` /
`sul_eye_d3d9_NNNN.jpg`, default pose, matched `rasterMajor=11`/`=5` pairs. Toggle `rasterMajor`
in `stage/client_d.cfg` (debug exe reads `client_d.cfg`) between captures; capture via existing
`ScreenShotHelper`. The structured dir + naming lets a scripted pixel-diff harness bolt on later
WITHOUT recapturing (D-05). CHAR-02/eyes and any "fixed" claim require a COMMITTED matched pair (D-07).

---

## Shared Patterns

### Pattern: `ConfigClientGraphics` gate for any shared-code diagnostic
**Source:** `ConfigClientGraphics.cpp:66,101,318`
**Apply to:** the census flag (§6). Anything that toggles shared `clientGraphics` behavior at
runtime goes through `ConfigFile::getKeyBool("ClientGraphics", key, default)`, NOT the plugin config.
Leaves the D3D9 path untouched when off.

### Pattern: `new char[len+1]` + null-terminate + dtor `delete[]` ownership
**Source:** VS `m_text` `ShaderImplementation.cpp:2305-2319`; `m_exe` dtor `:2783`
**Apply to:** the PSRC-retain member (§1, §2). Match the ctor-allocate / dtor-free lifecycle of `m_exe`.

### Pattern: feed text → `compilePixelShaderFromHlsl` → bind `m_d3dPS`
**Source:** `Direct3d11_PixelShaderProgramData.cpp:86-261`
**Apply to:** the recompile lane (§3). The helper is complete; the only new code is the
classification branch + the call + `return` on success. KEEP the magenta tombstone on failure.

### Pattern: D3DReflect (non-fatal) → map by NAME → `updatePS(slot,...)` full-struct write
**Source:** VS reflection `Direct3d11_VertexShaderData.cpp:611-676`; upload `Direct3d11.cpp:682-693`; `updatePS` `Direct3d11_ConstantBuffer.cpp:174-191`
**Apply to:** the constant upload (§4). Reflect the recompiled PS, map material/textureFactor by
reflected name to `PerMaterialCB` (PS slot b2), upload the full struct. Never hardcode D3D9 registers.
Reflection failure must be non-fatal (constants stay zero, no crash).

### Pattern: observable log channel selection
**Source:** `Direct3d11_PixelShaderProgramData.cpp:516-541` (preamble explains why)
**Apply to:** census (§6) and any diagnostic that must be visible under explorer launch. In the
PLUGIN use `ID3D11InfoQueue::AddApplicationMessage` (→ `d3d11-debug.log`); in SHARED `clientGraphics`
use `DEBUG_REPORT_LOG_PRINT` or a dedicated census stream (no InfoQueue access).

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| (none) | — | — | Every Phase 17 touch maps to an in-tree analog. The asm→HLSL secondary lane (only if the census shows asm PSRC) has NO in-tree precedent and would be NEW porting work — but D-09 makes it conditional, and RESEARCH.md flags it as the named landmine (NEVER re-assemble asm). If the census shows asm, the planner uses the recompile-lane analog (§3) as the structural template and ports asm→HLSL by hand. |

## Metadata

**Analog search scope:**
- `src/engine/client/library/clientGraphics/src/shared/` (ShaderImplementation.{cpp,h}, ConfigClientGraphics.cpp)
- `src/engine/client/application/Direct3d11/src/win32/` (PixelShaderProgramData, StaticShaderData, ConstantBuffer, VertexShaderData, Direct3d11.cpp, ConfigDirect3d11.cpp)
- `src/engine/client/application/ShaderBuilder/src/win32/PixelShaderProgramView.cpp`
- `docs/research/phase12-baseline/COMPARISON.md`

**Files scanned:** 9 source files + 1 doc, all line ranges VERIFIED this session.
**Pattern extraction date:** 2026-05-27
**Note:** working tree has uncommitted changes (none in the analog files touched here); line numbers
were re-verified against the live tree this session, not taken from RESEARCH.md on faith.

---

## Round-4 amendment (2026-05-29 — gap-closure replan)

The Round-4 cross-AI review (Codex + Cursor) corrected three factual claims that were
load-bearing in the Phase 17 plans pre-revision. These corrections cascade into the
gap-closure plans 17-05/06a/06b/07 and are folded into 17-VERIFICATION.md's scope-of-fix
narratives — but the corrections also retroactively scope-down the PATTERN claims above.

### Correction 1 (Round-4 HIGH-3 CONVERGED): "Phase 17 GAP" as written at line 236 is overstated

The §4 pattern callout above (`Direct3d11_ConstantBuffer::updatePS` → `materialDiffuse` zero-fill)
correctly identifies an instrumentation gap — the reflection-driven upload SHOULD populate the
material color fields. But char-select PSes (`h_simple_pp_ps20.psh`,
`h_color2_specmap_cbmp_ps20.psh` per `evidence/plan-17-04x-psrc-source-dump.txt`) consume
`textureFactor.rgb` + interpolated `COLOR0` (vertexDiffuse), NOT cbuffer `materialDiffuse` /
`materialEmissive`. Closing the materialDiffuse / materialEmissive write path (Plans 17-06a +
17-06b) is INSTRUMENTATION COMPLETENESS and lays infrastructure for FUTURE open-world shaders
that DO consume those cbuffer fields — but it does NOT materially drive char-select visuals.

The PRIMARY char-select visual driver is GAP-3 (asset-PS bind rate flip in Plan 17-07) +
the already-landed `textureFactor` write path. Plan 17-05 Task 5's per-CHAR-0x verdict
makes the instrumentation-vs-visual distinction explicit.

### Correction 2 (Round-4 HIGH-5 CONVERGED): `Direct3d11_HlslRewrite.cpp` Rule D source is NOT a c[N] → packedRegister[K].channel mapping table

The pattern-map's implicit assumption — that reading Rule D would reveal a translation table
from D3D9 c-register positions to packedRegister channels — does NOT hold. Rule D at
`Direct3d11_HlslRewrite.cpp:747-767` (verified read this session) only wraps `: register(cN)` →
`: packoffset(cN)` while preserving original declaration names. The `packedRegister0..4`
names in the reflection are the INPUT (`vertex_shader_constants.inc`) declaration names, not
a rewriter-emitted alias. Diffuse + emissive material values live INSIDE `userConstants`'s
opaque 272-byte region, NOT in packedRegisterN channels. Plan 17-06a defaults to Path B
(extending the discovery dump to walk userConstants's inner reflection); Path A (Rule D
source-read) is acknowledged as a dead end up-front.

### Correction 3 (Round-4 HIGH-4 CONVERGED): PSRC dominant form is parameter-list, not struct-bound

Plans that rewrite PS HLSL source (Plan 17-07) target `main()` parameter declarations as
PRIMARY (22/22 char-select PSes per `evidence/plan-17-04x-psrc-source-dump.txt` use
`float4 main(in TYPE NAME : SEMANTIC, ...)` syntax). The struct-bound form (`struct PSIn {
... } main(in PSIn inp)`) is a RARE-asset fallback with zero char-select usage. The
`buildHlslForVSOutputs` analog (§3 of the pattern map above) remains structurally correct
as a SORT-DISCIPLINE template, but its STRUCT-EMISSION shape is NOT the rewriter's output
target — the rewriter emits a reordered parameter list, not a struct.

### Net effect on the pattern map

The six pattern assignments (§1–§6) above remain correct as ROLE / DATA-FLOW / ANALOG maps.
What changes is the EXPECTED VISUAL OUTCOME per plan: 17-06a/06b deliver infrastructure
completeness; 17-07 delivers the primary char-select visual fix; 17-05 captures the
matched-pair evidence + verifier-produced verdict with explicit lane attribution.
