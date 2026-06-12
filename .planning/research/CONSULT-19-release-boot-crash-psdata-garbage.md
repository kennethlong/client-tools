# Consult: D3D11 release boot-crash — `m_program->m_graphicsData` is garbage (a freed MemoryBlockManager block) for a UI/post-FX shader, already at construct() time

Star Wars Galaxies client, D3D11 renderer port (`gl11`). Repo: `D:\Code\swg-client-v2` (branch `koogie-msvc-cpp20-base`, **available to you — read the code**). **Be skeptical / challenge the framing.** This project has repeatedly converged on wrong root causes via 3-AI consensus; we only trust hard evidence. A prior "root cause" for THIS exact crash was just FALSIFIED by a fresh full-memory dump (details below) — do not re-derive it.

## Symptom
`SwgClient_r.exe` (RELEASE, `gl11_r.dll`, `rasterMajor=11`) crashes at boot, `c0000005`, ~1s in, at the FIRST UI render. The DEBUG client (`SwgClient_d.exe`) works fully. Crash frame:

```
gl11_r!Direct3d11_StaticShaderData::apply+0x2a0   [Direct3d11_StaticShaderData.cpp @ 857]
  -> Direct3d11_StateCache::setStaticShader        [Direct3d11_StateCache.cpp @ 2405]
  -> PostProcessingEffectsManager::postSceneRender  [PostProcessingEffectsManager.cpp @ 252]
  -> CuiIoWin::draw -> Game::runGameLoopOnce -> Game::run -> ClientMain
```
Faulting instr: `mov edi,[eax+44h]` with `eax=1`. Crash `.txt` context: `ShaderTemplate_Iff: shader/ui_planet_sel_ordmantel_as8.sht`, `MainLoop:1`, `Player:none` (i.e. char/planet-select boot screen).

Line 857 is the iterator deref of `for (auto const &layout : layouts)` where `layouts = livePS->getReflectedCbufferLayouts()` (a `std::vector<Direct3d11_ReflectedPSCbufferLayout>`). `eax=1` is the vector's `begin` pointer = `0x1`.

## HARD EVIDENCE (full `/ma` dump `stage/release-gl11-crash.dmp`, taken AFTER the candidate fix was built into gl11_r.dll)
`this` (StaticShaderData) = `0x0a9705e0`, `passNumber=0`. Member offsets this build: `m_passVS@+0x00`, `m_passPS@+0x18`, `m_passPSSlot@+0x30`, `m_passMaterial@+0x54`.

Walking the per-pass pixel-shader pointers (idx 0):
| source | value |
|---|---|
| construct-time snapshot `m_passPS[0]` | **`0x21250000`** |
| slot `m_passPSSlot[0]` = `&pass->m_pixelShader->m_program->m_graphicsData` | `0x21250030` |
| live `*m_passPSSlot[0]` (re-read `m_graphicsData`) | **`0x21250000`** |

**snapshot == live == `0x21250000`.** So nothing is stale — the value did not change between construct() and apply(). And `0x21250000` is **NOT** a `Direct3d11_PixelShaderProgramData`. Its raw bytes:
```
21250000  2124ffd0 21250030 a2329380 00000000
21250010  00000001 01b9b8ec f50c49b0 054efee0   <- reflected-layouts vector @ +0x10 = {begin=0x1, end=0x01b9b8ec} == the crash regs
21250020  0ac4e9d0 21250680 0a96dee0 d5d1957f
21250030  21250000 21250060 c622ba08 00000000   <- m_graphicsData field (+0x30) holds 0x21250000
21250040  05326130 0a973780 05326130 64320000
21250050  21250014 21250010 6873702e 38629c00   <- "6873702e" = ".psh"
21250060  21250030 21250080 ...                 <- self-referential prev/next list nodes
```
The region is a **string / freelist arena**: embedded shader-filename fragments (`.psh`) and self-referential doubly-linked-list node pointers (`+0x00/+0x04`, `+0x30/+0x34`, `+0x60/+0x64` form a list). This looks exactly like a **`Direct3d11_PixelShaderProgramData` MemoryBlockManager block that was freed and reused** for CrcString / list-node storage.

Relevant: `Direct3d11_PixelShaderProgramData` is pool-allocated via `MemoryBlockManager` (`Direct3d11_PixelShaderProgramData.cpp:52 ms_memoryBlockManager`, `install()` @1022, `remove()` @1112, dtor @2023). Factory that creates it and is stored into the engine's `m_graphicsData`: `Direct3d11.cpp:669 return new Direct3d11_PixelShaderProgramData(pixelShaderProgram);`.

## The candidate fix that this evidence FALSIFIES
Prior root-cause: "construct() caches `m_passPS[idx]`; a boot-time device graphics-data remove/restore frees+RECREATES the gl11 PS-data to a NEW valid object, but the cache dangles → UAF; fix = read `m_graphicsData` live at apply (D3D9-faithful)." Implemented: `Direct3d11_StaticShaderData.cpp` now stores slot addresses `m_passVSSlot`/`m_passPSSlot` in construct() (`:427`, `:466`) and re-derives `liveVS`/`livePS` at apply() (`:782-789`), using `livePS` everywhere (`:831,:836,:857,...`).

**Why it's falsified:** snapshot == live, so there is no stale-vs-fresh difference to exploit. The engine's OWN `m_graphicsData` is the garbage value `0x21250000` — re-reading it live cannot recover a valid object because there is no valid object. The prior dump (now overwritten) validated the wrong object — it confirmed a `Direct3d11_ShaderImplementationData` had a valid vtable and ASSUMED the PS-program-data would too; it never directly read `m_program->m_graphicsData` live. (May also have been a different shader: prior was a "Dtl…sc" detail shader, this is the UI/PPEM planet-select shader.)

## D3D9 parity reference (the spec)
D3D9 renders this fine. `Direct3d9_StaticShaderData::Pass::apply()` (`Direct3d9_StaticShaderData.cpp:828`) reads `m_vertexShader->m_graphicsData` LIVE each apply — architecturally identical to our candidate fix — and binds the PS via the impl's live `Direct3d9_ShaderImplementationData::apply(pass)` (`:1055`). So D3D9 relies on the SAME "engine pass/program objects are stable, only the gl sub-object matters" assumption, and simply **never has a garbage `m_graphicsData` for this PPEM/UI shader.** The question is what D3D11 does differently in the PS-data lifecycle.

## Construct() snapshot guard (so you know what we capture)
`Direct3d11_StaticShaderData.cpp:459-462`: we only snapshot `m_passPS[idx]` when `pass->m_pixelShader && pass->m_pixelShader->m_program && pass->m_pixelShader->m_program->m_graphicsData` (non-null). So at construct() the pointer was non-null but ALREADY pointing at the freed/reused block `0x21250000`.

## Questions (cite file:line; the repo is yours to read)
1. **Lifecycle:** For a PPEM / UI post-process shader (`ui_planet_sel_*`, the post-scene composite), is `Direct3d11_PixelShaderProgramData` ever created (via `Direct3d11.cpp:669`) and stored into `ShaderImplementationPassPixelShaderProgram::m_graphicsData`? Or is `m_graphicsData` for these shaders set by a DIFFERENT path / left dangling? Trace who writes `m_program->m_graphicsData` and when, vs when `StaticShaderData::construct()` runs for the PPEM shader.
2. **Free + reuse:** Given the block is a freed MemoryBlockManager entry reused for string/list data, what frees the `PixelShaderProgramData` while the engine's `m_program->m_graphicsData` still points at it (so it's never nulled)? Look at `Direct3d11_PixelShaderProgramData::remove()`/dtor and the device graphics-data remove/restore boot sequence. Is the engine destroying+recreating the ShaderImplementation graphics data during boot but failing to update `m_program->m_graphicsData` (i.e. the dangling pointer lives in the ENGINE struct, not in our cache)?
3. **Why DEBUG works, RELEASE crashes:** layout/timing/heap-pattern sensitivity, or a real ordering difference (e.g. PPEM init order vs shader-data creation differs by config)? How would we tell which from the dump / code?
4. **Right fix:** Is the correct fix (a) ensure PPEM/UI PS-data is created (and not freed) so `m_graphicsData` is valid; (b) a validity guard — detect that `livePS`/`m_graphicsData` isn't a real `Direct3d11_PixelShaderProgramData` (e.g. vtable check, or `getReflectedCbufferLayouts()` sanity) and skip the material-upload loop / the whole apply for that pass; or (c) something else? If a guard, what's a SOUND validity test that won't mask a real bug (begin=1 is obviously bogus, but we want a principled check, not a magic-number reject — a prior value-threshold guard failed because bad pointers are high addresses)?
5. **Is `construct()` even snapshotting the right thing for PPEM shaders?** Should PPEM/post-FX shaders go through `StaticShaderData::construct()`/`apply()` at all, or do they have a separate bind path in D3D9 that D3D11 is incorrectly routing through the asset material-upload loop?

## Deliverable
The most likely root cause (with the file:line evidence chain for who creates/frees `m_program->m_graphicsData` for PPEM/UI shaders and why it ends up a freed block), a recommended fix (lifecycle correction preferred; guard only as interim, with a principled validity test), and a cheap verification (what to check in the next full dump / RenderDoc to confirm `m_graphicsData` is now a real PS-data object with a sane reflected-layouts vector). Call out anything in our framing you think is still wrong.
