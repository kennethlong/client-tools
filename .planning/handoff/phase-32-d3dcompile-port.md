# Handoff — Phase 32 (D3DX → d3dcompiler_47), resume at 32-02 Task 2

**Status:** 32-01 GATE complete + committed. 32-02 Task 1 complete but **UNCOMMITTED** (in working tree). Resume = 32-02 Task 2.
**Mode:** `/gsd:execute-phase 32` interactive-inline (user authors review at each checkpoint; I write code inline, build myself, user does the in-game render smokes). Worktrees OFF, sequential, on `master`.
**Last session:** 2026-06-16.

---

## Resume command

`/gsd:execute-phase 32` — discovery will see `32-01-SUMMARY.md` (skip 32-01) and resume at the first incomplete plan (32-02). The 32-02 Task-1 edits below are **already on disk uncommitted** — do NOT redo them; continue at Task 2.

---

## DONE — 32-01 GATE: PASS (committed)

- Commits: `9b71aef9b` (deliverables: 32-CENSUS.md, d3dassemble_probe.cpp, probe-output.txt) + `0228783c8` (ROADMAP/STATE tracking).
- **Verdict: PASS** — `D3DAssemble` (d3dcompiler_47) is **byte-for-byte identical** to `D3DXAssembleShader` on the SWG asm dialect (6/6 probed shaders incl. `lit` opcode + `if b#` branch; `CreateVertexShader` S_OK on every blob). Inverts the prior pessimism (Phase 27 reverted; RESEARCH said "no existence-proof").
- **Consequence: Wave 1 = 32-03** (port `:567` asm `D3DXAssembleShader → D3DAssemble`). **32-04 is SKIPPED.** 32-05 can fully retire Fix-A (both paths off D3DX).
- Corpus refreshed: **286 `.vsh` = 190 `//hlsl` + 96 `//asm`, 0 unclassified** (holds). 12 extracted `.vsh` on disk (docs said "16" — corrected).

## DONE (uncommitted) — 32-02 Task 1: rewriter port + build wiring

**Working-tree state (THIS is Task 1 — do not redo):**
- `??` `src/engine/client/application/Direct3d9/src/win32/Direct3d9_HlslRewrite.h` (lifted from `d3d9-fixb-d3dcompile-wip`, re-namespaced `Direct3d9_HlslRewrite`)
- `??` `src/engine/client/application/Direct3d9/src/win32/Direct3d9_HlslRewrite.cpp` (lifted + **Rule D fully removed**)
- `M` `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` (added `<ClCompile ...Direct3d9_HlslRewrite.cpp>`)
- `M` `src/engine/client/application/Direct3d9/build/win32/Direct3d9_vsps.vcxproj` (same)

What was decided/applied:
- **Rule D removed** (the SM4 `cbuffer:register(b0)`/`packoffset` wrap) — verified `grep cbuffer.*register(b0)` = 0, `tryRuleDWrap` = 0. Rationale comment in the cpp (D3D9 flat `register(cN)` is native; engine pushes by index `Direct3d9_StateCache.cpp:237`). Rules A/B/C verbatim; two-pass `DEBUG_FATAL` bounds guards intact (10).
- **Rule E = INERT** — corpus probe: **0 of 190 `//hlsl` shaders use `#pragma def` or `c95`** → leave the rule, it matches nothing (lowest-risk decision-table row). No gating needed.
- `d3dx9.lib` retained in both vcxprojs (D-05). `Direct3d9_ffp.vcxproj` untouched.
- **WIP-branch disposition: KEEP** — AGENTS.md lists `d3d9-fixb-d3dcompile-wip` as "do not delete" (parked for x64). This plan re-derives its scaffold on master with the two corrections it lacked (Rule D off, Fix-A kept).
- **Commit held** until Task 2's build verifies the rewriter compiles (same build-before-commit discipline as 32-01).

---

## TODO — 32-02 Task 2 (the heavy chunk) + Task 3

### Task 2 — port the call site, fix overrides, build, 3 audits

1. **`Direct3d9_VertexShaderData.cpp`** — base on the WIP diff, regenerate it with:
   `git diff master..d3d9-fixb-d3dcompile-wip -- src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp`
   The WIP diff already has: `#include <d3dcompiler.h>`; the include-rewrite-before-cache (rewrite in the `Include` ctor → cache stores rewritten bytes, satisfies Cursor C2); the `:455` `: register(vN)` → `;` strip; `applyToMainSource` on `m_compileText`; flags = `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY` only.
   **THREE CORRECTIONS vs the WIP diff (do NOT lift it verbatim):**
   - **KEEP Fix-A (D-04):** the WIP *deleted* `compileVertexShaderFpGuarded` (the `__try/__except` SEH). Instead, **retain the SEH guard and call `D3DCompile` INSIDE it** (belt-and-suspenders this plan; removed in 32-05). Do NOT delete the SEH helper.
   - **vs_1_1 guard:** `:391-395` has `else target = "vs_1_1"`. D3DCompile dropped vs_1_1 → add `DEBUG_FATAL`/assert `getShaderCapability() >= 2.0` OR promote unconditionally to `vs_2_0`. No silent null-VS (D-06). State which in SUMMARY.
   - The WIP `fixB-failed-shader.txt` DEV diagnostic (marked REVERT) — optional; keep as a clean failure-dump aid or drop. Plan doesn't require it.
   - Do NOT touch the asm path `:521-567` (still `D3DXAssembleShader`). Do NOT add `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR`.
   - Do NOT lift the WIP's vcxproj postbuild change (it dropped the `d3dcompiler_47.dll` stage copy — KEEP the master postbuild copy).
2. **9 overrides** `stage/override/vertex_program/{c_simple,envmask_specmap_c,tfcl,tfcl_2uv,tfcl_4uv,tfcl_env,tfcsl,tfcsl_2uv,tfcsl_2uv_env}.vsh` — strip `: register(vN)` from each `#define DECLARE_textureCoordinateSets` body (incl. `tfcl_4uv`'s 4×). **Edit via Edit/bash, NEVER PS Set-Content (UTF-8 BOM crashes Release boot).**
3. **Build** 5-target `Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient` Debug|Win32 `/nodeReuse:false` from PowerShell (`$env:MSBUILD`), delete `SwgClient_d.exe` first. Grep log: `unresolved external symbol`=0 (the `/FORCE` false-clean trap), `error C`=0 incl. the gl11/Direct3d11 target (shared-header ABI guard).
4. **3 audits → `.planning/research/vsh-probe/hlsl-constant-audit.txt`:** (i) constant-layout (c0/c16 stay flat globals, no cbuffer/packoffset — proves Rule D gone); (ii) **v# input-signature disasm** — `tfcl` inputs must be the gapped `dcl_position v0 / dcl_normal v3 / dcl_color v5 / dcl_texcoord v7` (VSVR map, NOT sequential v0/v1/v2/v3) — diff vs the D3DXCompileShader baseline blob — **this is the exact WIP stall point**; if divergent, the lever is a D3D9-specific Rule-C narrowing that preserves the first-colon `register(vN)` on struct members; (iii) flag audit (BACKWARDS_COMPATIBILITY only, no PACK_MATRIX_ROW_MAJOR).

### Task 3 — checkpoint:human-verify (USER)
Boot `stage/SwgClient_d.exe` (reads `client_d.cfg`, `rasterMajor=5`, searchPath includes `stage/override/`) → char select → Tatooine bump/dot3 scene. Compare vs Fix-A reference (`db83b0f5c`): no missing/garbled/black/displaced geometry, overrides compile clean. `head -c 8 stage/client_d.cfg | xxd` must be `23 20` (no BOM).

### Then close-out
Commit 32-02 (explicit paths: the rewriter .h/.cpp, both vcxprojs, VertexShaderData.cpp, 9 overrides, hlsl-constant-audit.txt + SUMMARY) **only after the build is green**. Write `32-02-SUMMARY.md`. Then Wave 1 = **32-03** (asm port), then 32-05 (close-out, dual-renderer smoke, Fix-A retirement).

---

## Reusable tooling facts (so you don't re-derive)

- **Probe build recipe (x86):** `vcvars32.bat` (VS2022 Community `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat`) → `cl /EHsc /MD /D_CRT_SECURE_NO_WARNINGS /I"<repo>\src\external\3rd\library\directx9\include" <src> /link /LIBPATH:"...\directx9\lib" d3d9.lib d3dx9.lib user32.lib gdi32.lib advapi32.lib legacy_stdio_definitions.lib`. The in-tree directx9 SDK has `d3d9.h`/`d3dx9.h`/`d3dx9.lib` but NOT `d3dcompiler.h`/`d3dcommon.h` (those come from the Windows SDK via vcvars). `d3dcompiler_47.dll` is x86 in `stage/`; `d3dx9_*.dll` runtime in SysWOW64.
- **`d3dassemble_probe.cpp`** (committed) is the tooling shape to extend for the Task-2(G)(ii) v#-signature audit — it already wires `D3DDisassemble` via GetProcAddress + a HAL device + `D3DXAssembleShader` baseline. For the HLSL audit, call `D3DCompile`/`D3DXCompileShader` and `D3DDisassemble` the input-signature lines.
- **`dumpbin`** must run via the PowerShell tool, not Git-Bash (`/exports` gets MSYS-mangled to `C:\Program Files\Git\exports`).
- **TRE corpus enumeration:** `swg_pipeline.tre_reader` (`list_tre`/`read_tre_payload`) over `D:/Code/SWGSource Client v3.0/*.tre`; Python 3.14.
- **Corpus skew note:** the Phase-27 extracted `diffuse.inc` (ILM pack) references `cExtLtData_*` aliases the extracted `registers.inc` doesn't define — a stale-sample version-skew, not a real defect (both assemblers reject identically without them). Harmless to the gate; the probe supplies them.
