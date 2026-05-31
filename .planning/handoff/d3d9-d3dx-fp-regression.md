# Handoff: D3D9 D3DXCompileShader FP-crash regression (Fix A verification)

**Updated:** 2026-05-31
**Branch / worktree:** `koogie-msvc-cpp20-base` @ `D:\Code\swg-client-v2-d3d11` (the renderer source-of-truth)
**Status:** Fix A cherry-picked onto koogie (`db83b0f5c`). Consistent Release D3D9 stack built from koogie (clean link, 0 unresolved externals) and staged. **BOOT TEST PASSED 2026-05-31 14:23** — `SwgClient_r.exe` + `gl05_r.dll` boot D3D9 (`rasterMajor=5`) clean past the pre-login first frame, no crash dump → **Bug 2 confirmed ABSENT on koogie** (it was a swg-blender-m16 partial-17-07 artifact). Remaining: in-game Fix A verification at Mos Eisley bump/dot3 spot.

> **Branch rule (Kenny, reaffirmed 2026-05-31):** NONE of the D3D9 or D3D11 renderer work belongs on `swg-blender-m16` (that branch is the Blender/IFF pipeline workstream only). All renderer work happens in THIS koogie worktree. See memory `feedback_d3d11_work_belongs_in_koogie_worktree`.

---

## Pick up here (restart)

1. Confirm you are in the koogie worktree: `git -C D:\Code\swg-client-v2-d3d11 branch --show-current` → `koogie-msvc-cpp20-base`.
2. There is **no older D3D9 handoff** — this thread previously lived only in todos + memory. Resume context:
   - Memory `project_d3d9_d3dxcompileshader_fp_crash` (root cause + Fix A + debugger recipe)
   - Todo `.planning/todos/pending/2026-05-31-current-tree-d3d9-boot-crash-17xx-wip.md` (the c0000005 boot crash — was on swg-blender-m16's partial 17-07)
   - Todo `.planning/todos/pending/2026-05-31-port-d3d9-shader-compile-to-d3dcompile.md` (Fix B, deferred)
3. Check the in-flight Release build (see "Build to verify" below); when it lands, boot and verify.

---

## The two coupled bugs

### Bug 1 — D3DXCompileShader FP crash (`0xC0000090`) — Fix A SHIPPED, runtime-UNVERIFIED

The D3D9 stack (gl05 default + gl07 vsps) statically compiles **D3DX from source**. Built with the modern MSVC toolchain (VS2022-class, v14.50), D3DX unmasks FP exceptions and trips an invalid-op during its post-compile FP cleanup (`_control87`/`FWAIT` in `CPreProcessor::~CPreProcessor`) while lazily compiling certain **bump/dot3 vertex shaders** mid-render. Retail VS2003-built D3DX did not. The shader bytecode is fully produced BEFORE the cleanup fault.

- **Decisive control:** pristine SWG Source VS2003 client (`D:\Code\SWGSource Client v3.0`, `rasterMajor=5`) renders the SAME shader/asset/spot CLEAN on this exact machine/server/assets/character; ours crashes. Only the toolchain differs. → our-build-side, not environmental.
- **Fix A** (orig `99a4ac5a1` on swg-blender-m16 → cherry-picked to koogie as **`db83b0f5c`**): `compileVertexShaderFpGuarded` in `Direct3d9_VertexShaderData.cpp` wraps `D3DXCompileShader` in `__try/__except` (FP SEH codes `0xC000008D..0xC0000093`), `_fpreset()` + `_clearfp()`, keeps the compiled bytecode. Helper-extracted to dodge C2712. Covers gl05+gl07 (shared `#ifdef VSPS` file).
- **Fix B (deferred):** port D3D9 compile to `D3DCompile` (immune; the D3D11 path already uses it) and remove the SEH guard. Todo `2026-05-31-port-d3d9-shader-compile-to-d3dcompile`.

### Bug 2 — current-tree D3D9 boot crash (`c0000005`) — likely a swg-blender-m16 artifact

On **swg-blender-m16** (partial/stale 17-01..17-07 D3D11 shader-pipeline WIP), a clean Release D3D9 build null-calls into d3d9.dll on the first pre-login frame: `setPixelShaderConstants(index=0, n=5)` ← `applyLights_vertexShader` ← `selectLights` ← first post-process `drawTriangleFan`. The 05-27 pre-17 binaries boot clean. Suspected cause = the shared `ShaderImplementation.h` change (17-01 inserted `m_psrcText`/`m_psrcLen` mid-struct, shifting `m_graphicsData`'s offset) consumed inconsistently.

**Hypothesis (to confirm via the build below):** koogie has the COMPLETE/verified 17 (17-08/17-09/`bb4b13a00`), so Bug 2 likely does NOT reproduce on koogie. If koogie boots D3D9 clean, Bug 2 was a swg-blender-m16-only artifact and the blocker on verifying Fix A evaporates.

---

## Build to verify (consistent Release D3D9 stack from koogie)

Why a full stack: the 17-01 `ShaderImplementation.h` struct-offset shift means a koogie-built `gl05_r.dll` can NOT pair with the 05-27 `SwgClient_r.exe` (ABI mismatch). Build both from koogie.

```powershell
$msb = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
$sln = "D:\Code\swg-client-v2-d3d11\src\build\win32\swg.sln"
& $msb $sln /t:SwgClient /p:Configuration=Release /p:Platform=Win32 /m /nodeReuse:false
```

- Building the `.vcxproj` standalone FAILS (`LNK1181: cannot open input file 'archive.lib'`) — deps aren't built in Release in this worktree. Use the **solution** target so the dependency graph builds the libs in order.
- Outputs: `src/compile/win32/Direct3d9/Release/gl05_r.dll` + `src/compile/win32/SwgClient/Release/SwgClient_r.exe`. Postbuild/stage copy lands them via the `stage` junction (`worktree/stage -> D:\Code\swg-client-v2\stage`).
- `stage/` currently holds the **05-27 pre-17** `SwgClient_r.exe` + `gl05_r.dll` (the known-good D3D9 boot baseline) — these get replaced by the koogie Release outputs.

---

## Runtime verification plan

1. **Boot test (answers Bug 2):** run `stage/SwgClient_r.exe` Release at `rasterMajor=5` (Release exe reads `client.cfg`, NOT `client_d.cfg`). Clean boot to login = Bug 2 absent on koogie.
2. **Fix A verification (answers Bug 1):** get to the bump/dot3 shaders that FP-crashed — **Tatooine Mos Eisley**, creature/wearable normal-map materials (dewback saddle, hair/moufhair). With Fix A, the SEH guard should catch the FP fault, reset the FPU, and keep rendering instead of crashing. (In-game navigation to the spot is the manual/Kenny-driven part — see memory `project_d3d9_charselect_capture_via_release_stack` for the capture workflow.)
3. Run the client NORMALLY to test the fix — a first-chance debugger break masks the SEH handling.

**Debugger recipe (yes/no, no symbols needed):** `windbg -c "sxe 0xc0000090; g" SwgClient_r.exe`. Launchers in repo root: `dbg-d3d9.cmd`, `dbg-d3d9-swgsource.cmd`. fpcw/fpsw are per-thread — check thread 0 (`~0s`, render thread).

---

## Open side-questions

- **gl05 vs gl07 reference choice:** the SWG Source community DEFAULT is `rasterMajor=7` (gl07, auto-picked for modern GPUs); we used gl05 (`rasterMajor=5`) as the D3D9 parity reference. Settle which is the D3D11-parity baseline. Both run VSPS on modern HW.
- **swg-blender-m16 history:** Fix A + Phases 18/19 renderer commits left in place there (pushed, ahead 35) — NOT rewritten. Just stop adding renderer work to that branch.

## References

- Memory: `project_d3d9_d3dxcompileshader_fp_crash`, `feedback_d3d11_work_belongs_in_koogie_worktree`, `project_d3d9_charselect_capture_via_release_stack`, `feedback_debug_exe_reads_client_d_cfg`, `project_rastermajor_values`, `feedback_parity_work_start_from_reference_sequence`
- Code: `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp` (Fix A guard @ ~83-110, call site ~476)
- Cherry-pick: `db83b0f5c` (`-x` of `99a4ac5a1`)
