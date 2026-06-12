# Handoff: D3D11 release-client boot crash — UAF FIX FALSIFIED (still crashing) — 2026-06-02

> **UPDATE 2026-06-02 (latest, supersedes everything below): the UAF root-cause + live-deref fix is FALSIFIED. Release STILL boot-crashes.**
>
> Drove `dbg-d3d11-crash.cmd` via windows-mcp → fresh full dump `stage/release-gl11-crash.dmp` (19:01), with the live-deref fix already built into gl11_r.dll (17:13). Crash is IDENTICAL: `gl11_r!Direct3d11_StaticShaderData::apply+0x2a0` line **857** (the `for (auto const& layout : layouts)` deref), `eax=1`, via `PostProcessingEffectsManager::postSceneRender:252` → `setStaticShader` → `apply`. Context shader `ui_planet_sel_ordmantel_as8.sht`, MainLoop:1, Player:none.
>
> **Heap walk (this=0x0a9705e0, passNumber=0; offsets m_passPS@+0x18, m_passPSSlot@+0x30):**
> - snapshot `m_passPS[0]` = **0x21250000**
> - slot `m_passPSSlot[0]` (`&...m_graphicsData`) = **0x21250030**
> - live `*slot` (re-fetched m_graphicsData) = **0x21250000**
> - **snapshot == live == 0x21250000** → nothing is stale → **NOT a use-after-free of the cached pointer → the live-deref fix is INERT here.**
> - `0x21250000` is a string/freelist arena, NOT a `Direct3d11_PixelShaderProgramData`: bytes contain `.psh` filename fragments (`6873702e` @ +0x48) + self-referential list-node ptrs; the reflected-layouts vector at `0x21250010` = `{begin=0x1, end=0x01b9b8ec}` (exactly the crash regs).
>
> **Conclusion:** `m_program->m_graphicsData` is garbage **already at construct()**. Reading it live can't recover a valid object. The prior 16:50 analysis validated the wrong object (`ShaderImplementationData` 0x0aaf0360 was valid) and never directly read the PS-program-data live; possibly also a different shader (detail vs UI/PPEM). Consensus-trap — broken by capture-and-verify.
>
> **CONSULT DONE (CODEX+Cursor, dump-verified) → `.planning/research/CONSULT-19-release-boot-crash-psdata-garbage-SYNTHESIS.md`.** Refined+verified root cause: the slot `m_passPSSlot[0]=&program->m_graphicsData` (0x21250030) is an **interior pointer into a freed/reused engine `ShaderImplementationPassPixelShaderProgram`**. `m_graphicsData @ +0x20` (symbol-verified) → Program base 0x21250010 = incoherent (m_exe/m_psrcLen garbage); `*slot`=0x21250000 vtable 0x2124ffd0 ≠ gl11 real PS-data vtable 0x6106b9f0. Actual shader = PPEM `ms_copyShader`=`2d_texture.sht` (`ui_planet_sel` in the .txt is a ShaderTemplateList breadcrumb). `Direct3d11::flushResources` is a no-op (`:267-269`) → device-restore UAF narrative is FALSE; rewrite the StaticShaderData.{h,cpp} comments.
>
> **Fix status:** live-deref change UNCOMMITTED (`Direct3d11_StaticShaderData.{h,cpp}` still `M`), ineffective (reads freed interior memory). **Fix direction (both consults):** (1) preferred — wrapper must `fetch()`/`release()` the engine pass pixel-shader/Program so the slot stays valid for its lifetime; (2) interim — principled validity guard (engProgram->m_graphicsData==livePS && livePS vtable==0x6106b9f0 && PS-data m_program(+0x04)==engProgram) → else skip the Plan-17-03 reflection/material loop & fall back (D3D9 never runs that loop). **OPEN "why":** what frees/reuses the `2d_texture` Program during boot (RELEASE-only) — break on its `release()`/dtor + refcount→0 before first postSceneRender (suspect `asynchronousLoaderRelease`, ShaderImplementation.cpp:2697-2714). DEBUG client still works.

---

# (prior) Handoff: D3D11 release-client boot crash — ROOT-CAUSED + FIX BUILT (pending runtime verify) — 2026-06-02

> **UPDATE 2026-06-02 (later same day): ROOT-CAUSED via full /ma dump; fix implemented + gl11 rebuilt. NOT a Heisenbug / not N bugs — a single use-after-free.**
>
> **Root cause:** `Direct3d11_StaticShaderData::construct()` caches the *derived* graphics-data pointers `m_passVS[]`/`m_passPS[]` = `pass->m_*Shader->...->m_graphicsData`. A device-level **graphics-data remove/restore during boot frees and RECREATES** the gl11 per-program objects (the engine updates `m_program->m_graphicsData` to the new objects), but the `StaticShaderData` snapshot is never refreshed → `m_passPS[idx]` dangles to a freed 0x50-byte MemoryBlockManager block (reused for a `PersistentCrcString`). `apply():824` iterating `getReflectedCbufferLayouts()` (begin=0x1) → `c0000005`. Crash MOVES (PPEM postSceneRender, UI flushRenderQueueQuads) because **every** snapshot is stale.
>
> **Dump proof** (`stage/release-gl11-crash.dmp`, captured via `dbg-d3d11-crash.cmd`): `eax=1` at `mov edi,[eax+44h]`; m_passPS[0]=0x20d7fa10; that block +0x00=0x20d7f9e0 (heap, NOT a gl11 0x6106xxxx vtable), +0x14=`SwgClient_r!PersistentCrcString::vftable`, on a 0x50-spaced freelist. Heap search: ONLY m_passPS holds it; the live engine graph (StaticShader 0x0a80ee50 / impl 0x20d7f580 / `Direct3d11_ShaderImplementationData` 0x0aaf0360 = valid vtable) does NOT → engine recreated it; re-fetch returns a valid object.
>
> **Fix (D3D9-faithful, BUILT 2026-06-02):** D3D9 never caches the derived pointer — it reads `m_graphicsData` LIVE at apply (`Direct3d9_StaticShaderData.cpp:829`). Mirrored it: `construct()` now also stores the ADDRESS of the engine owner's `m_graphicsData` slot (`m_passVSSlot`/`m_passPSSlot`, same double-indirection as `Stage::m_texture`); `apply()` re-derives `liveVS`/`livePS` from the live slot and uses them at every deref (binding + reflection + bytecode + SRV remap). The construct-time `m_passVS/m_passPS` snapshot is kept only for construct-time texcoord work, the size invariant, and the `usesVertexShader()` presence check. Files: `Direct3d11_StaticShaderData.{h,cpp}`. gl11_{r,d}.dll rebuilt + staged. **NEXT: run `dbg-d3d11-crash.cmd` (or just launch the release client) — if it boots past the first UI render, fixed; then commit.** Repro tooling: `dbg-d3d11-crash.cmd` + `stage/dbg-d3d11-fulldump.cdb`. Headless cdb can't reach the render (DirectInput `CreateDevice` FATALs with no foreground window) — needs an interactive desktop launch.

---

## ORIGINAL HANDOFF (pre-root-cause) follows — superseded by the UPDATE above

# (original title) D3D11 release-client boot crash (systemic post-FX/UI PS-data corruption) — 2026-06-02

**Branch/checkout:** `koogie-msvc-cpp20-base` in MAIN checkout `D:\Code\swg-client-v2` (the only git worktree now; the old `swg-client-v2-d3d11` worktree is gone).
**Committed + pushed this session:** `5fce7bb83` (depth fix, //asm VS fallback, tfcl/tfcsl plumbing, prior Phase-19 fixes, Options-window crash fix). 15 files, Direct3d11/* + SwgCuiOptUi.cpp only.
**Suggested next-session start:** a cross-AI consult (CODEX + Cursor) on the systemic PS-data corruption root, with the callstacks + suspects below.

---

## TL;DR
This session shipped big D3D11 wins (world/interior **depth** + tfcl/tfcsl **lighting**, both runtime-verified on the RELEASE client earlier). Then, chasing a cosmetic cape bug, I needed the Options menu → it crashed (separate UI-asset bug, fixed) → to get the fix in I **rebuilt + restaged `SwgClient_r.exe`**, which **boot-crashes**, and the rebuild's postbuild **clobbered the working May-31 release exe** (built from the now-deleted worktree, never committed → unrecoverable). The boot crash is a **systemic, layout-sensitive use-after-free / uninitialized-memory bug in the post-FX / UI shader `apply()` path** — NOT one shader. The **DEBUG client works fully** (depth, lighting, world, play) and is the current working client.

## What's committed & working (5fce7bb83)
- **Depth fix** — `Direct3d11_RenderTarget.cpp` binds the shared screen DSV (`::46`) on the full-screen scene RT (`::323`). The whole world had no depth test before. `Direct3d11_Device::getDepthStencilView()` added. RUNTIME-VERIFIED (capture9-12, interiors + exteriors occlude correctly).
- **//asm VS fallback** — `Direct3d11_VertexShaderData::compileVariant()` compiles a generic world-transform fallback VS for legacy //asm vertex programs. `skippedNullVS` 38927→0; interiors/sky draw. `finalizeVariantFromBytecode()` shared tail; `isAssembly()`; StateCache forces fallback-PS lane for asm VS.
- **tfcl/tfcsl //hlsl** — 7 reauthored VS give per-vertex Gouraud lighting. **These live in `stage/override/vertex_program/*.vsh` (runtime, NOT git-tracked)** + `client.cfg`/`client_d.cfg` `searchPath_00_1x` override entries. Repack into a TRE when finalized (swg-blender-plugin tre tools). RUNTIME-VERIFIED.
- **Options-window crash fix** — `SwgCuiOptUi.cpp`: post-v3.0 widgets (`checkShowToolbarCommandCooldownTimer`, auto-sort, vendor-examine, buff-icon sliders) made optional so Options opens instead of FATAL-ing on the missing widget. (Useful once release boots; harmless now.)

## THE OPEN PROBLEM — release boot crash (systemic PS-data corruption)
`SwgClient_r.exe` (HEAD build) crashes at boot, `c0000005`, in **`gl11_r!Direct3d11_StaticShaderData::apply`**, dereferencing a corrupt `m_passPS[idx]` / its corrupt members.

**Two distinct crash sites observed (the crash MOVES = systemic, not one shader):**
1. `apply()` material-upload loop (`StaticShaderData.cpp:~824/840`), via `PostProcessingEffectsManager::postSceneRender` (`PostProcessingEffectsManager.cpp:252`) → `setStaticShader` (`StateCache.cpp:2405`). `m_passPS[idx]`'s `getReflectedCbufferLayouts()` vector had `begin = 0x1` (garbage). `eax=1` at the element deref.
2. After a guard fixed #1, it moved to `apply()` line ~1017 (the `writeVarByName` lambda iterating `layout.Vars`), via **`CuiLayerRenderer::flushRenderQueueQuads`** (UI quad rendering) — a *different* shader, *nested* vector (`layout.Vars`) garbage.

**Diagnosis:** `m_passPS[idx]` (set in `construct():451` from `pass->m_pixelShader->m_program->m_graphicsData`) points to a **freed / uninitialized `Direct3d11_PixelShaderProgramData`** object (high address, passes null-check, but its std::vectors are garbage). The code's own comment at `StaticShaderData.cpp:843` documents this as a **known layout-sensitive Heisenbug** (Plan 17-03; "uninitialized-memory or timing-dependent issue in the setStaticShader call chain"). The lost May-31 working exe had a code layout that didn't expose it; my rebuild's layout does.

**What I tried (all reverted — they were whack-a-mole, NOT committed):**
- Value-threshold guard on `m_passPS[idx]` (reject `< 0x10000`): failed — the bad pointer is a *high* freed address.
- Probe the PS object's reflected-layouts `begin` pointer / size, null `passPS` if corrupt: fixed crash #1 but the crash moved to #2 (nested `layout.Vars`, different shader). Confirmed it's systemic — per-site guards won't converge.

**Conclusion:** the root is *why* `m_passPS[idx]` (and `m_passVS`?) point to corrupt PS objects across multiple shaders. Candidates to investigate next session:
- **UAF:** the PS program's `m_graphicsData` (Direct3d11_PixelShaderProgramData) is freed while the cached `StaticShaderData` / `ms_active` still references it. Look at PPEM + UI shader lifecycle and when gl11 frees PixelShaderProgramData.
- **Uninitialized:** post-FX / UI shaders whose PS `m_graphicsData` was never properly created (gl11 returns a sentinel / uninit object), so `construct()` caches garbage.
- **Init-order / static-lifetime:** `ms_active` cache or a static in `apply()` interacting badly across the post-FX + UI passes.

## How to debug (works great)
- **Symbolicate the dump with cdb** (x86, matches 32-bit client):
  `"C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe" -z <dump.mdmp> -y "<symdir1>;<symdir2>" -i "<same>" -lines -c ".ecxr; kn 16; q"`
  Symbol dirs: `src\compile\win32\SwgClient\Release` (exe+pdb) **and** `src\compile\win32\Direct3d11\Release` (gl11 pdb). Pass the FULL dump path (don't prefix `stage\` if the var already has it).
- Crash dumps land in `stage/SwgClient_r.exe-unknown.0-*.{mdmp,txt}`. The `.txt` has the exception + addr; the `.mdmp` has the stack.
- Next step: dump `this` (the StaticShaderData) + `m_passPS` vector contents at the crash frame to confirm UAF vs uninit; trace where that PS `m_graphicsData` is created/freed for the post-FX + UI shaders.

## Current binary / client state
- **DEBUG client `SwgClient_d.exe` = WORKING** (boots, plays, depth + lighting correct). It's the client to use until release is fixed. (Backed up: `binary-backups/working-debug-20260602/`.)
- **RELEASE `SwgClient_r.exe` = boot-crashes** (HEAD build, staged). The broken build preserved at `binary-backups/broken-newbuild-20260602/SwgClient_r.exe.broken` for analysis.
- `binary-backups/pre-p19-nanhunt-20260531/SwgClient_r.exe` (May-27) is **ABI-incompatible** with current gl11 (predates commit 3667b3b06 `ShaderImplementation.h` member add) — do NOT use it.
- **Staged `gl11_{d,r}.dll` (16:02) match the committed clean source** (depth + VS-fallback + tfcl + Rule F; speculative guards reverted).

## Build / run
- gl11 plugin: `& "$env:MSBUILD" "D:\Code\swg-client-v2\src\engine\client\application\Direct3d11\build\win32\Direct3d11.vcxproj" /p:Configuration=Release|Debug /p:Platform=Win32 /nodeReuse:false /v:minimal /m` (auto-stages gl11_{r,d}.dll).
- Client EXE: `...\src\game\client\application\SwgClient\build\win32\SwgClient.vcxproj` (Release→SwgClient_r.exe, Debug→SwgClient_d.exe; auto-stages). **The exe project does NOT auto-rebuild dependency libs** — build `clientObject`/`swgClientUserInterface`/etc. vcxproj first if you changed them, then the exe.
- **ALWAYS back up `stage/SwgClient_{r,d}.exe` before a client rebuild** (postbuild overwrites it). The working exe was lost this way.

## Follow-ups / cleanup noted (NOT blocking)
- Diagnostic scaffolding still in the committed tree (pre-existing, harmless): `P19_SKIP_AUDIT=1` (StateCache), `P19_THREAD_AUDIT`/`P19_DISCARD_ONLY=0` (DynamicVertexBufferData), the per-compile vs-disasm dump in finalizeVariantFromBytecode. Clean up when convenient.
- The cape "texture tearing" (player robe + robed NPCs, jagged black spikes) is a **skinned-mesh** bug (confirmed NOT a shadow), capture-invisible (live-only). Deprioritized cosmetic; revisit after release boots.
- Repo hygiene: commit/back up known-good client exes — the working one was never committed and couldn't be recovered.
