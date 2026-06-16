# 32-CENSUS — `.vsh` corpus classification + D3DAssemble export evidence + Wave-1 GATE

Phase 32 (SHADER-01 / D3DX → d3dcompiler_47). Plan **32-01**, Tasks 1–3.
This is the **critical early de-risk gate**: it refreshes the `//hlsl` vs `//asm` census and empirically
answers whether `D3DAssemble` (d3dcompiler_47) accepts the SWG `.vsh` asm dialect and produces bytecode
semantically equivalent to the `D3DXAssembleShader` baseline. The tri-state **GATE VERDICT** (bottom)
selects the Wave-1 plan: PASS / PASS-WITH-NOTE → **32-03** (asm port) · FAIL-WITH-FOLLOWUP → **32-04** (fallback).

---

## 1. Reconciled corpus counts (the 12-vs-16 ambiguity closed to ground truth)

Three distinct numbers, stated explicitly so the cross-doc drift is closed:

| Basis | Count | How obtained |
|-------|-------|--------------|
| **FULL corpus** (TRE archives) | **286 `.vsh`** = **190 `//hlsl` + 96 `//asm`**, 0 unclassified | Authoritative enumeration this session (below) |
| **EXTRACTED on-disk sample** | **12 `.vsh`** (11 `//asm` + 1 `//hlsl`) in `.planning/research/vsh-extract/` | `ls` (verified) |
| **BASELINE-PROBED** | **3** (`c_simple`, `tfcl`, `cloudlayer`) | The 32-01 Task-2 baseline probe set |

> **Drift note (review fix #9):** prior Phase-32 docs said "**16 extracted**". The on-disk truth is **12 `.vsh`**
> (the dir also holds 1 `.psh` + 11 `.inc` modules = 24 files total; the "16" appears to have counted non-`.vsh`
> files or a stale extraction). Per AGENTS.md "files are truth, docs lag" the count is reconciled here to the
> ground-truth **12 extracted `.vsh`**. The "16 extracted" figure is hereby corrected. Of the 12: 11 are `//asm`,
> 1 (`a_simple_vs20_for_ps20.vsh`) is `//hlsl`.

### Full-corpus refresh (Phase-27 framing HOLDS)

```
TRE archives scanned: 209 (errors/skipped: 0)
UNIQUE .vsh logical names across corpus: 286
CLASSIFIED unique .vsh: hlsl=190  asm=96  other/unclassified=0  total=286
```

Method (reproducible): `swg_pipeline.tre_reader.list_tre` over all 209 searchPath TREs in
`D:/Code/SWGSource Client v3.0/*.tre`; for each unique `*.vsh` logical name (first-win by archive order),
`read_tre_payload` then classify on the first non-blank directive line (`//hlsl` vs `//asm`). **0 shaders left
unclassified** (D-06: no shader unclassified, no VS silently skipped). The Phase-27 **190 `//hlsl` + 96 `//asm`
of 286** split is confirmed unchanged.

---

## 2. The 9 `stage/override/vertex_program/*.vsh` loose HLSL overrides (already shadow TRE `//asm` originals)

Verified `ls stage/override/vertex_program/*.vsh` = **9 files** (D-02 reauthor targets for 32-02):

```
c_simple.vsh   envmask_specmap_c.vsh   tfcl.vsh   tfcl_2uv.vsh   tfcl_4uv.vsh
tfcl_env.vsh   tfcsl.vsh   tfcsl_2uv.vsh   tfcsl_2uv_env.vsh
```

These are loose-file overrides on the client `searchPath` that shadow the TRE `//asm` originals with `//hlsl`
reauthorings. They are 32-02's concern (the HLSL port + the macro-expanded `: register(vN)` removal), not 32-01's.

---

## 3. RAW `D3DAssemble` export evidence (review fix Codex — reproducible, not prose)

**Resolved DLL path:** `D:\Code\swg-client-v2\stage\d3dcompiler_47.dll` (staged, 4 147 664 bytes)
**Machine:** `14C machine (x86)` — 32-bit, matching the Win32 client (correct for the gl05/gl07 link).
**Import-lib path (Windows Kit, x86):** `C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\d3dcompiler.lib`
(also present under `10.0.22621.0` and `10.0.19041.0`).

`dumpbin /exports stage\d3dcompiler_47.dll` (run via PowerShell to avoid MSYS `/exports` path-mangling) —
verbatim relevant lines:

```
  Section contains the following exports for D3DCOMPILER_47.dll
          1    0 001745D0 D3DAssemble
          4    1 001743B0 D3DCompile
          3    2 00174420 D3DCompile2
          5    3 00174490 D3DCompileFromFile
         13    9 000B18D0 D3DDisassemble
         11    A 000BFB60 D3DDisassemble10Effect
         12    B 000B1960 D3DDisassemble11Trace
         14    C 000B1900 D3DDisassembleRegion
```

- **`D3DAssemble` = ordinal 1, hint 0, undecorated export name** — confirmed present in the staged x86 DLL.
- **`D3DDisassemble` = ordinal 13** — present (needed for the Task-2 token-compare fallback).
- The `d3dcompiler.lib` symbol is C++-mangled (`?D3DAssemble@@...`), so an `extern "C"` decl will NOT match the
  `.lib` import → the probe and the later 32-03 port resolve `D3DAssemble` via **`GetProcAddress` on the
  undecorated DLL export** (static-local resolve-once cache), sidestepping the mangling mismatch (Sonnet L2).

---

## 4. Construct flag-list — BINDING on 32-01 Task 2 (review fix #6)

The 3-shader baseline (`c_simple`/`tfcl`/`cloudlayer`) does NOT exercise every construct present across the 11
extracted `//asm` shaders. The probe in Task 2 **MUST** additionally cover each construct below. (`c_simple` and
`tfcl` are byte-identical; `cloudlayer` is the rich inline-opcode shader.)

| # | Construct | First seen in | Runtime-exercised? | Probe representative | Why baseline misses it |
|---|-----------|---------------|--------------------|----------------------|------------------------|
| F1 | **`lit` opcode** (partial-lighting macro-op) | `diffuse_specular.inc` | **YES** — body is outside the `#if`, runs unconditionally | **`tfcsl.vsh`** | baseline includes only `diffuse.inc` (no `lit`); `cloudlayer` is hand-rolled (no `lit`) |
| F2 | **4-texcoord declaration set** (`dcl_texcoord2/3` → `v9/v10`, outputs `oT2/oT3`, `maxTextureCoordinate 3`) | `tfcl_4uv.vsh` | **YES** | **`tfcl_4uv.vsh`** | `cloudlayer` reaches only `dcl_texcoord1`/`oT2`; baseline never declares `v9/v10` or writes `oT3` |
| F3 | **`if b#` / `endif` static boolean branch** (`if cLightData_*_enabled`) | `diffuse_specular.inc` + `registers.inc` `b0..b7` | **NO at runtime** — gated by `#if VERTEX_SHADER_VERSION >= 20`, and the **runtime asm path does NOT define `VERTEX_SHADER_VERSION`** (see note) | `tfcsl.vsh` assembled a 2nd time **with `VERTEX_SHADER_VERSION=20` added** | bonus dialect-coverage data point only |

### Why F3 is tool-path-only, not runtime-binding (important)

`Direct3d9_VertexShaderData.cpp:556-567` (the runtime asm `D3DXAssembleShader` call) pushes ONLY the
per-texcoord `vTextureCoordinateSet*` defines, `maxTextureCoordinate`, and `TARGET`→`vs_2_0`. It does **NOT**
define `VERTEX_SHADER_VERSION`. The offline **ShaderBuilder** tool (`VertexShaderProgram.cpp:148-151`) is the
only place that defines `VERTEX_SHADER_VERSION` (=`11`). Therefore, at runtime, `#if VERTEX_SHADER_VERSION >= 20`
is **false** → the `if`/`endif` boolean-branch *lines* in `diffuse_specular.inc` are stripped, while their
*bodies* (the lighting math **including `lit`**) remain and execute unconditionally. So:

- **F1 (`lit`) is runtime-binding** — it survives the `#if` strip and must assemble under the runtime define set.
- **F3 (`if b#`) is NOT runtime-binding** — the runtime never sees it. It is probed only as a bonus (with
  `VERTEX_SHADER_VERSION=20` explicitly added) to record whether D3DAssemble would accept SWG's boolean-branch
  dialect should a future path enable it. A F3 rejection does **not** by itself force FAIL-WITH-FOLLOWUP, because
  the runtime asm path 32-03 ports never emits it. (Recorded as a NOTE in the verdict if it occurs.)

**Probe set for Task 2 (binding):** `c_simple`, `tfcl`, `cloudlayer` (baseline) **+ `tfcsl` (F1 `lit`) + `tfcl_4uv`
(F2 4-uv)**, each assembled with the **runtime** define set (`TARGET=vs_2_0`, no `VERTEX_SHADER_VERSION`); **plus**
one bonus run of `tfcsl` with `VERTEX_SHADER_VERSION=20` added (F3).

---

## 5. `//hlsl` vs `//asm` classification (extracted sample — representative; full-corpus split in §1)

| Extracted `.vsh` | Class | Notable constructs |
|------------------|-------|--------------------|
| `a_simple_vs20_for_ps20.vsh` | `//hlsl` | HLSL `vs_2_0` (the 32-02 path, not 32-01's) |
| `c_simple.vsh` | `//asm` | baseline; `diffuse.inc` (byte-identical to `tfcl`) |
| `tfcl.vsh` | `//asm` | baseline; `diffuse.inc` |
| `cloudlayer.vsh` | `//asm` | baseline; hand-rolled `m4x4/m4x3/m3x3/dp3/rsq/mul/sub/mad/max/mov/add/exp/rcp`, dot3 tangent setup |
| `tfcl_2uv.vsh` | `//asm` | `diffuse.inc`, 2 texcoords |
| `tfcl_4uv.vsh` | `//asm` | **F2** — 4 texcoords (`v7..v10`, `oT0..oT3`) |
| `tfcl_env.vsh` | `//asm` | `diffuse.inc` + `env_t1.inc` (`dp3/add/mad` — covered by cloudlayer) |
| `tfcsl.vsh` | `//asm` | **F1** — `diffuse_specular.inc` (`lit`) |
| `tfcsl_2uv.vsh` | `//asm` | `diffuse_specular.inc` (`lit`) |
| `tfcsl_2uv_env.vsh` | `//asm` | `diffuse_specular.inc` (`lit`) + `env_t1.inc` |
| `envmask_specmap_c.vsh` | `//asm` | `diffuse_specular.inc` (`lit`) + `env_t1.inc` |
| `gradient_sky.vsh` | `//asm` | hand-rolled `mov/dp3/rsq/mul/sub/m4x4`, `cUserConstant` (opcodes all covered by cloudlayer) |

Opcode coverage check: the union of `cloudlayer` + `diffuse.inc` (via `c_simple`) covers
`m4x4 m4x3 m3x3 dp3 rsq mul sub mad max mov add exp rcp dst`. The only runtime opcode NOT in that union is
**`lit`** (F1). All `dcl_*`/output registers up to `oT3` are covered once F2 (`tfcl_4uv`) is added.

---

## GATE VERDICT

**GATE VERDICT: PASS — asm port proceeds (Wave 1 = 32-03).**

`D3DAssemble` (d3dcompiler_47) accepts the SWG `.vsh` asm dialect and produces bytecode **byte-for-byte
identical** to the `D3DXAssembleShader` baseline on every probed shader — **zero byte-variance, zero token
divergence** (so the result is a clean PASS, not PASS-WITH-NOTE), and `CreateVertexShader` returns **S_OK** on
every D3DAssemble blob. This empirically settles the open question (32-RESEARCH Pitfall 2 / Open-Q1): there WAS
no existence-proof that D3DAssemble accepts the SWG dialect (Restoration precompiles bytecode; the API is not in
public `d3dcompiler.h`) — the probe now provides one, and it is exact equivalence.

### Per-shader evidence (`d3dassemble_probe.cpp` → `probe-output.txt`, run from `stage/`, x86, HAL device)

| Probe shader | Covers | D3DAssemble HRESULT | D3DXAssembleShader | size (both) | FNV hash (both) | size+hash | CreateVertexShader |
|--------------|--------|---------------------|--------------------|-------------|-----------------|-----------|--------------------|
| `c_simple.vsh` | baseline + `diffuse.inc` | `0x00000000` | `0x00000000` | 1384 | `0x8FF52C5FFB47A76C` | **MATCH** | **S_OK** |
| `tfcl.vsh` | baseline | `0x00000000` | `0x00000000` | 1384 | `0x8FF52C5FFB47A76C` | **MATCH** | **S_OK** |
| `cloudlayer.vsh` | baseline (inline opcodes) | `0x00000000` | `0x00000000` | 588 | `0x505D7190D3BBD323` | **MATCH** | **S_OK** |
| `tfcsl.vsh` | **F1 `lit` opcode** | `0x00000000` | `0x00000000` | 1540 | `0x43A83FDB9F71CCD9` | **MATCH** | **S_OK** |
| `tfcl_4uv.vsh` | **F2 4-texcoord (`v7..v10`/`oT0..oT3`)** | `0x00000000` | `0x00000000` | 1456 | `0x15C69904F985B18F` | **MATCH** | **S_OK** |
| `tfcsl.vsh` +`VERTEX_SHADER_VERSION=20` | **F3 bonus `if b#` branch** | `0x00000000` | `0x00000000` | 1636 | `0xB30D608DFFF200D5` | **MATCH** | **S_OK** |

`SUMMARY: pass=6  pass-with-note=0  excluded=0  fail(runtime-binding)=0  fail(bonus/F3)=0`

The disasm **token-compare fallback never had to run** — every blob size+hash-MATCHED outright, so there was no
byte-variance to adjudicate. Auto-FAIL is RESERVED for an assemble-error HRESULT where the baseline succeeds, a
`CreateVertexShader` rejection, or a token divergence — **none occurred**.

### Harness note (corpus version-skew, not a dialect issue)

A first probe run hit `X2005: invalid register 'cExtLtData_parallelSpec_0_tangentColor'` on the `diffuse.inc`-using
shaders (c_simple/tfcl/tfcl_4uv) — **identically on BOTH D3DAssemble and D3DXAssembleShader**. Root cause: the
Phase-27 extracted `diffuse.inc` (an ILM-pack copy, from `ILM_visuals.tre`) references `cExtLtData_*` tangent-light
aliases that the extracted `registers.inc` (from `data_other_00.tre`) does not define — a **corpus include
version-skew**, not a D3DAssemble incompatibility (the trusted baseline rejected it the same way). The probe
supplies the 3 missing aliases (`cExtLtData_*` → free slots `c52/c53/c54`) to **both** assemblers identically, so
the equivalence comparison stays valid and the F2 shader assembles. This skew is a property of the stale extracted
sample; it does not affect the gate conclusion (equivalence holds wherever both assemble; both fail identically
otherwise).

### Scope + recovery path

**Scope:** this gate is **PASS-ON-SAMPLE** — it certifies the 3 baseline + the F1/F2 (and bonus F3) flag-listed
constructs over the 12-extracted sample, **NOT** the full 96-`//asm` corpus.
**Recovery rule (verbatim, review fix #3):** _"If 32-03's render-smoke fails on a NON-PROBED shader, roll back 32-03, re-route to 32-04, and append the failing construct to 32-ASM-FOLLOWUP.md."_

### Downstream consequence

- **Wave 1 = 32-03** (port the `//asm` `:567` call site `D3DXAssembleShader` → `D3DAssemble`). Skip 32-04.
- Because the gate is PASS (not FAIL-WITH-FOLLOWUP), the asm path does **not** stay on D3DXAssembleShader, and
  there is no must-land-before-Phase-33 asm follow-up from this branch.
- 32-03's Task-1 double-execution guard will assert this `PASS` token AND `test ! -f 32-04-SUMMARY.md` before editing `:567`.
