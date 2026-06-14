# Phase 27 — Asm-Shader Census (HARD-05 Success Criterion 1)

**Recorded:** 2026-06-14
**Census source:** `D:/Code/SWGSource Client v3.0/*.tre` (the shipped shader source our engine reads at runtime)
**Census method:** Python TRE reader + payload header inspection (`//hlsl` vs `//asm` marker on the first bytes of each `.vsh`)
**Census run:** tool-verified 2026-06-13 (see `27-RESEARCH.md` § "D-02 Verification (a) Asm census"). This artifact RECORDS that already-completed census; it does NOT re-run the extraction.

> **Why this comes first.** HARD-05 mandates an asm-shader census as the FIRST step of the
> `D3DXCompileShader → D3DCompile` port so the port provably does NOT silently null the assembly
> vertex-shader path. `D3DCompile` compiles HLSL only; if the 96 `//asm` shaders were dropped, their
> vertex shaders would become NULL → `CreateVertexShader` is never called → those draws are skipped →
> lightsabers, decals, and detail-mapped surfaces vanish with no FATAL. This is exactly the failure
> mode the STATE.md blocker **"D3DCompile asm gap"** warns against. Recording the census as a committed
> scoping artifact satisfies HARD-05 Success Criterion 1.

## Census Results

| Metric | Count |
|--------|-------|
| Unique `.vsh` (vertex shaders) | **286** |
| `//hlsl` `.vsh` (HLSL — compiled via `D3DXCompileShader`, the port target) | **190** |
| `//asm` `.vsh` (assembly — assembled via `D3DXAssembleShader`) | **96** |
| Unique `.psh` (pixel shaders — consumed as pre-compiled `PEXE`, **NOT** runtime-compiled, out of scope) | **454** |
| Read errors | **0** |

Total accounted: 190 `//hlsl` + 96 `//asm` = **286** unique `.vsh`. The 190 HLSL shaders are the
`D3DCompile` migration surface (Plan 02); the 96 asm shaders are the path that must NOT be dropped.

Our HLSL is clean ASCII (`//hlsl vs_2_0` header — e.g. `c_simple.vsh` first bytes
`2f 2f 68 6c 73 6c` = `//hlsl`), so the HLSL path is a near-1:1 in-place compiler swap on text we
already own (D-01-R). The asm shaders carry the `//asm` marker and route to the `D3DXAssembleShader`
branch.

## 96-Asm-Consumer Sample List

These real, rendered-content shaders depend on the `//asm` path (detail-mapped surfaces, lightsabers,
decals). Nulling the assembly path would skip their draws:

- `a_detail.vsh`
- `a_detail_bump.vsh`
- `a_detail_specmap.vsh`
- `saberbase.vsh`
- `saber_blade_cap.vsh`
- `a_replace0_decal.vsh`
- `c_2blend_decal_lowmem.vsh`
- `a_2blend_specmap_dirt_p2.vsh`

(Eight of the 96; full set is the `//asm`-marked subset of the 286 `.vsh` census.)

## Scoping Conclusion

The asm path — `D3DXAssembleShader` at `Direct3d9_VertexShaderData.cpp:567` — handles **96** shaders
and **MUST NOT be dropped**. Per **D-02-R (REVISED 2026-06-14)**, the asm path **STAYS on
`D3DXAssembleShader` + the SEH guard** for this phase. This satisfies HARD-05's requirement that the
"Phase-19 SEH guard be retained for any path still on D3DX."

- **This phase (Phase 27):** port the **HLSL path only** (190 shaders) from `D3DXCompileShader` →
  `D3DCompile`. The 96 `//asm` shaders are left untouched on the proven `D3DXAssembleShader` + SEH
  path.
- **Plan 03 validates** that the asm path is left untouched (the port does not silently null it) by
  confirming saber/decal/detail geometry still renders.
- **Deferred to the x64 milestone:** the asm → `D3DAssemble` port (the LOW-confidence SWG-asm-dialect
  item, RESEARCH Assumption A3) and the complete removal of `d3dx9.lib` from the D3D9 runtime (D3DX is
  used by 4 other gl05/gl07 files — `Direct3d9.cpp`, `Direct3d9_TextureData.cpp`,
  `Direct3d9_RenderTarget.cpp`, `Direct3d9_StaticShaderData.cpp` — so dropping the lib is intractable
  this phase regardless of the asm path).

## References

- **HARD-05** (`.planning/REQUIREMENTS.md`) — Success Criterion 1: census produced first.
- **STATE.md blocker "[v2.3 — D3DCompile asm gap]"** — "Census the asm (`.vsh` / `D3DXAssembleShader`)
  count BEFORE starting HARD-05. `D3DCompile` compiles HLSL only; silently dropping the asm path nulls
  the VS → skipped draws. Keep `D3DXAssemble` + SEH guard until the asm path is explicitly handled."
- `27-RESEARCH.md` § "D-02 Verification (a) Asm census" — the authoritative tool-verified numbers.
- `27-CONTEXT.md` D-02-R (REVISED 2026-06-14) — HLSL-only port scope; asm stays on
  `D3DXAssembleShader` + SEH.
- Compile surface: `Direct3d9_VertexShaderData.cpp` — HLSL compile (`D3DXCompileShader`, call ~:477)
  and asm assemble (`D3DXAssembleShader`, :567).
