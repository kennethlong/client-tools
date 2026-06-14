# Phase 27: D3DCompile Port - Context

**Gathered:** 2026-06-13
**Status:** Ready for planning

<domain>
## Phase Boundary

HARD-05: get the **live D3D9 runtime shader compile (gl05/gl07)** off the modern-toolchain-faulting
`D3DXCompileShader`/`D3DXAssembleShader` path and onto `D3DCompile`. The chosen approach is **not** a
minimal in-place `D3DXCompileShader → D3DCompile` swap that babysits an asm path on D3DX. Instead:
**adopt SWG Restoration's already-modernized (HLSL) shader set — extracted from their TREs — and pair
it with our own `D3DCompile` call** (mirroring the D3D11 plugin's existing `D3DCompile` + include
handler). If Restoration's shaders are all HLSL, the asm `.vsh` path dissolves and D3DX is fully
retired from the runtime compile.

**In scope:** the live gl05 (D3D9 default) + gl07 (D3D9 vsps) runtime compile path in
`Direct3d9_VertexShaderData.cpp`; extracting + integrating Restoration's HLSL shaders; writing the
`D3DCompile` call; D3D9 visual parity held.

**Out of scope (this phase):** ShaderBuilder offline editor; CORNERSNAP probe removal / door-snap
fix; the x64 port; gl06 FFP (no HLSL compile); gl11 D3D11 (already on `D3DCompile`).
</domain>

<decisions>
## Implementation Decisions

### Port approach — lift Restoration shaders, write our own D3DCompile call
- **D-01:** The port is **Restoration's TRE shaders + our own `D3DCompile` call**, NOT a 1:1
  `D3DXCompileShader → D3DCompile` swap and NOT lifting Restoration's plugin *source* (we have their
  `gl0X_r.dll` as binaries only). We extract Restoration's modernized (assumed HLSL / non-D3DX)
  shader assets from `D:\SWG Restoration\SwgRestoration_*.tre` and feed them to a `D3DCompile` call we
  write, mirroring the **existing** D3D11-plugin pattern (`Direct3d11_CompileIncludeHandler` +
  `Direct3d11_ShaderCache`).
- **D-02 (asm path dissolves — VERIFY FIRST):** The premise is that Restoration's shaders are HLSL,
  so `D3DCompile` covers 100% and the asm `D3DXAssembleShader` path
  (`Direct3d9_VertexShaderData.cpp:567`) is **retired**. The researcher's **first job** is to extract
  Restoration's shaders and confirm format + coverage vs our current shader set (how many were asm
  `.vsh`, do the HLSL replacements render identically). FALLBACK: if any shaders cannot be replaced
  by a Restoration HLSL equivalent, keep just those on `D3DXAssembleShader` + the SEH guard — but the
  goal is full D3DX retirement from the runtime compile.
- **D-03 (SEH guard):** `D3DCompile` is immune to the modern-toolchain post-compile FP fault that
  Fix-A (`db83b0f5c`) guards, so the SEH wrapper (`compileVertexShaderFpGuarded`) can be **relaxed /
  removed on the HLSL path** once it is on `D3DCompile`. Retain it only on any residual D3DX path.
  Decide during validation (D-05).

### CORNERSNAP / door snap boundary
- **D-04:** Phase 27 is the D3DCompile port ONLY. The CORNERSNAP `_DEBUG` probes
  (`CollisionResolve.cpp`, `CellProperty.cpp`) stay **untouched**. Re-baseline the door snap on the
  new D3DCompile build as an **observation only** (the port is expected NOT to fix the 32-bit
  build/codegen door transient). Probe removal + any door-snap fix are deferred to a future **x64
  port** (not yet scheduled; x64 also addresses the chronic 32-bit OOM and matches Restoration's
  x64 build).

### Validation bar
- **D-05:** Parity = **dual-renderer boot smoke** (rasterMajor=5 D3D9 AND =11 D3D11) to
  char-select / in-game, **plus a targeted check on the exact Tatooine bump/dot3 vertex-shader spot
  Fix-A was created for** (`db83b0f5c` — confirm it renders clean post-port). Evaluate whether the
  SEH guard can be dropped on the now-`D3DCompile` HLSL path. Boot invariant applies: Debug exe reads
  `client_d.cfg`, Release exe reads `client.cfg` (Claude owns the rasterMajor toggle on switches).

### ShaderBuilder editor
- **D-06:** OUT of scope. Leave the ShaderBuilder Qt editor's `D3DXCompileShader`/`D3DXAssembleShader`
  calls untouched (pre-broken editor, not a shipping build target — consistent with prior phases).
  Port only the live gl05/gl07 runtime.

### Claude's Discretion
- Exact `D3DCompile` wiring (d3dcompiler version, include-handler reuse, shader cache integration)
  is for research/planning to mirror from the D3D11 plugin.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase definition
- `.planning/ROADMAP.md` — Phase 27 "D3DCompile Port" entry
- `.planning/REQUIREMENTS.md` — HARD-05 acceptance text (line 16)
- `.planning/STATE.md` — HARD-05 census-first decision; boot invariant; /FORCE false-pass guard

### Port target (the live D3D9 runtime compile)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp` — HLSL compile
  via `compileVertexShaderFpGuarded`→`D3DXCompileShader` (helper ~:87-110, call site :477) AND asm
  `D3DXAssembleShader` (:567); the Fix-A SEH guard + `_clearfp`/`_fpreset` live here. **The TODO at
  ~:85 explicitly names this port: "Fix B, deferred: port D3D9 shader compile to D3DCompile".**

### Reference implementation to mirror (already on D3DCompile)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_CompileIncludeHandler.{cpp,h}` — the
  `D3DCompile` include handler pattern
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderCache.cpp` — `D3DCompile` +
  caching wiring
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp` / `Direct3d11.cpp` — how the D3D11 plugin invokes `D3DCompile`

### Shader source to lift (Restoration)
- `D:\SWG Restoration\` — Restoration client install. **Shaders to extract:** `SwgRestoration_*.tre`
  (+ `SwgRestoration.toc`). **Reference plugins (binary only — behavior reference, NOT source to
  lift):** `gl05_r.dll`, `gl06_r.dll`, `gl07_r.dll`, and `x64\gl05_r.dll` (Restoration runs x64).
  NOTE: `D:\SWGEmu-Client\SWGEmu\` is a *different*, retail-era install — Restoration is the lift
  source, not SWGEmu.

### History / context
- Commit `db83b0f5c` — Fix-A SEH guard (the modern-toolchain `D3DXCompileShader` FP fault this port
  supersedes); memory `project_d3d9_d3dxcompileshader_fp_crash`.
- Shader extract/repack + override workflow: swg-blender-plugin TRE tools; loose
  `stage/override/vertex_program/` + `client.cfg` `searchPath_00_10` (memory
  `project_d3d11_world_view_anomalies_phase19` — VS-fallback + shader override workflow).
</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **D3D11 plugin's `D3DCompile` path** (`Direct3d11_CompileIncludeHandler` + `Direct3d11_ShaderCache`)
  — a working, in-repo `D3DCompile` + include-handler + cache implementation to mirror for the D3D9
  port. This is the single biggest accelerator; the port is largely "do what gl11 does, in gl05/gl07."
- **Shader override/extract workflow** — established TRE extract/repack tooling + the loose
  `stage/override/` + `searchPath` mechanism for dropping reauthored shaders in (Phase 19).

### Established Patterns
- gl05/gl07 **statically compile D3DX from source**; built with the modern MSVC toolchain, D3DX
  unmasks FP exceptions and faults post-compile on certain bump/dot3 shaders (Fix-A territory).
  `D3DCompile` (via `d3dcompiler_*.dll`) sidesteps the whole static-D3DX problem.
- The plugin keeps the compiled bytecode even when the FP fault fires (Fix-A); a clean `D3DCompile`
  removes the need for that workaround on the HLSL path.

### Integration Points
- `Direct3d9_VertexShaderData.cpp` is the single runtime entry point for both HLSL compile and asm
  assemble — the focused surface for the port.
- Restoration's extracted shaders enter via the override/searchPath workflow (or repacked into a TRE).
</code_context>

<specifics>
## Specific Ideas

- Kenny's framing (locked): "we can get the shaders from Restoration TRE extracts, they are already
  using the non-D3DX paths, why would we not lift and shift everything." → adopt Restoration's
  modernized shaders rather than hand-converting our asm set; write our own `D3DCompile` call.
- Restoration uses the **identical gl05/gl06/gl07 plugin architecture** as this client and ships an
  **x64** build — corroborates the deferred-x64 direction for the door snap + 32-bit OOM.
- Lift source is **`D:\SWG Restoration`** (NOT `D:\SWGEmu-Client`, which is a separate retail-era install).
</specifics>

<deferred>
## Deferred Ideas

- **Asm-path migration as a standalone effort** — only needed if Restoration's shaders don't fully
  cover our asm set; otherwise it dissolves. (Fallback: keep residual asm on D3DX + SEH guard.)
- **CORNERSNAP probe removal + door-snap fix** — deferred to a future x64 port (not scheduled). The
  D3DCompile port is expected not to fix the 32-bit-codegen door transient.
- **x64 port** — separate future milestone; addresses the door snap + chronic 32-bit OOM; matches
  Restoration's shipping x64 build.
- **ShaderBuilder editor `D3DCompile` migration** — not planned (pre-broken Qt editor, non-shipping).

</deferred>

---

*Phase: 27-d3dcompile-port*
*Context gathered: 2026-06-13*
