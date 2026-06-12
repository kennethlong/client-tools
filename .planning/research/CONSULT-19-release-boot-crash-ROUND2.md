# Consult ROUND 2: D3D11 release boot-crash — `2d_texture` program's `m_graphicsData` is a dangling freelist pointer; HOW, given the source says it's set to a valid object and never freed?

Star Wars Galaxies client, D3D11 renderer port (`gl11`). Repo: `D:\Code\swg-client-v2` (branch `koogie-msvc-cpp20-base`, **available — read the code**). **Be aggressively skeptical. We have gone VERY deep (8 runtime experiments) and explicitly want you to find what we are MISSING / a wrong assumption.** This project's failure mode is 3 AIs converging on a wrong root cause; Round 1 of this very consult produced a confident conclusion that we then FALSIFIED with runtime evidence (below). Challenge the framing.

## The crash (unchanged)
`SwgClient_r.exe` (RELEASE, `gl11_r.dll`, rasterMajor=11) `c0000005` at boot, ~1s, first PPEM composite draw. DEBUG client works fully.
```
gl11_r!Direct3d11_StaticShaderData::apply+0x2a0  [Direct3d11_StaticShaderData.cpp @ 857]   (for-loop over livePS->getReflectedCbufferLayouts(), eax=1)
 -> Direct3d11_StateCache::setStaticShader [StateCache.cpp @ 2405]
 -> PostProcessingEffectsManager::postSceneRender [PostProcessingEffectsManager.cpp @ 252]   (binds ms_copyShader = shader/2d_texture.sht)
```

## Round-1 conclusion we FALSIFIED
R1 (both CODEX+Cursor) said: cached `m_passPS`/slot dangles after a device remove/restore frees+recreates PS-data → UAF; fix = read `m_graphicsData` live. **Falsified:** (a) `Direct3d11::flushResources` is a no-op (`Direct3d11.cpp:267-269`); (b) snapshot==live (the live-deref fix is inert); (c) `remove()` never fires (below). Do not re-derive R1.

## What we did (8 experiments) and what each PROVED
1. **Re-launch w/ live-deref fix built** → still crashes; `snapshot m_passPS[0] == live *slot == 0x21250000` (identical) → NOT a stale-cache problem.
2. **Engine chain-walk (crash dump):** `StaticShaderData(0x0a9705e0)` → `m_shader`(StaticShader, coherent, round-trips) → `m_implementation 0x2124fb70` (ShaderImplementation, coherent: m_users=1, valid m_graphicsData/m_pass) → `m_pass`→`pass[0] 0x2124fbb0` → `m_pixelShader 0x0a0df4a0` → **`m_program 0x21250010`**, `m_fileName="pixel_program/2d_texture.psh"`, `m_referenceCount=1`. The whole chain ABOVE m_graphicsData is coherent.
3. `program(0x21250010)->m_graphicsData` (@ +0x20, symbol-verified) `= 0x21250000`. Its vtable `0x2124ffd0` ≠ the real `gl11_r!Direct3d11_PixelShaderProgramData::vftable` → **not a live PS-data**.
4. **Program/PS-data destructor watch** (`~ShaderImplementationPassPixelShaderProgram`, `release`, `asynchronousLoaderRelease`, `~Direct3d11_PixelShaderProgramData`) — booted to crash, **ZERO fires**. → Nothing goes through a C++ destructor. (Falsifies "freed via dtor".)
5. **Creation watch** (`Graphics::createPixelShaderProgramData`, `Direct3d11Namespace::createPixelShaderProgramData_impl`, `Direct3d11_PixelShaderProgramData::ctor`): `2d_texture` PS-data **IS created correctly** — `GFX-CREATE`→`GL11-IMPL`→`PSDATA-CTOR this=0aa1dee0 prog=21250010 name=pixel_program/2d_texture.psh` (20 programs total, each created once). So a valid PS-data object exists.
6. **Pool teardown watch** (`Direct3d11_PixelShaderProgramData::remove` + `install`): `install` fires once at startup (`Direct3d11::install`→`Graphics::install`→`ClientMain`), `remove` **NEVER fires**. → The MBM pool is not torn down via the gl11 API.
7. **Hardware write-watch on the m_graphicsData slot** (`ba w4` armed correctly per `bl`, set at the PS-data ctor for 2d_texture): printed **`slotval-at-ctor = 0x21250000`** and then **ZERO writes** to the slot through to the crash.
8. **Arena identity:** `0x21250000 → 0x2124ffd0 → 0x2124ff80 …` is a `0x50`-strided freelist chain (`0x50 == sizeof(Direct3d11_PixelShaderProgramData)`), with `PersistentCrcString::vftable` debris nearby. So `m_graphicsData` points into the **PS-data MemoryBlockManager freelist** (a freed/recycled 0x50 block), in the same `0x2124–0x2125` arena as the program object itself.

## THE CENTRAL PARADOX (please explain this — we think we're missing something)
Source — `ShaderImplementation.cpp:2754-2776`:
```cpp
ShaderImplementationPassPixelShaderProgram::ShaderImplementationPassPixelShaderProgram(CrcString const &fileName)
:  m_referenceCount(1), m_fileName(fileName), m_exe(NULL), m_psrcText(nullptr), m_psrcLen(0),
   m_graphicsData(NULL)                                    // (1) init list: m_graphicsData = NULL
{
   if (... || Graphics::getShaderCapability() >= ShaderCapability(1,1)) {
      Iff iff(fileName.getString());
      load(iff);                                           // (2)
      m_graphicsData = Graphics::createPixelShaderProgramData(*this);   // (3) line 2770: assign
   }
   ms_programMap.insert(ProgramMap::value_type(&m_fileName, this));     // (4) line 2773
}
```
Our hardware watch armed on `&this->m_graphicsData` (= `prog+0x20 = 0x21250030`) **inside step (3)** — i.e. inside the `Graphics::createPixelShaderProgramData(*this)` call, at the nested `Direct3d11_PixelShaderProgramData` ctor, BEFORE the assignment in step (3) stores its result. At that instant:
- `m_graphicsData` reads **`0x21250000`**, but per step (1) it should be **NULL**.
- After step (3) the engine MUST store the result (`0aa1dee0`) into `this->m_graphicsData`. **The hardware watch on that exact slot saw ZERO writes** — the assignment in step (3) never writes `0x21250030`.

`createPixelShaderProgramData(*this)` passes `this` by reference, so the arg (`poi(esp+4)=0x21250010`) == `this` == the assignment target's object. So the store should hit `0x21250030`. It doesn't. And the slot is non-NULL garbage even before the store. **How is this possible?** Candidate angles we want you to evaluate or refute (and add your own):

1. **Release `DEBUG_FATAL` is a no-op.** Line 2775's `DEBUG_FATAL(!result, ("already existed!"))` and line 2773's `ms_programMap.insert` — in RELEASE a duplicate insert silently fails (`result=false`, ignored). Could there be **two `ShaderImplementationPassPixelShaderProgram` instances for `2d_texture`** — one correctly built + in the map, and an orphan at `0x21250010` (referenced by the StaticShaderData) whose `m_graphicsData` was never validly set? (Creation watch only saw ONE create, but could a second be created via a path that doesn't hit `Graphics::createPixelShaderProgramData` — e.g. a copy/move, or pre-cached?)
2. **`0x21250010` is recycled memory / stale pointer.** Was the program constructed correctly earlier, then freed via a **bulk MemoryBlockManager path that bypasses `~ShaderImplementationPassPixelShaderProgram`** (so dtor watch sees nothing), its 0x50-block returned to the PS-data freelist, and the StaticShaderData left holding a stale `0x21250010`? If so, what bulk-frees engine `ShaderImplementationPassPixelShaderProgram` objects without their dtor — and why is the program arena (`0x2124–0x2125`) shared with the `Direct3d11_PixelShaderProgramData` MBM freelist at all? (Are engine programs and gl11 PS-data unexpectedly sharing an allocator/arena?)
3. **`*this` aliasing / object layout.** Is `m_graphicsData` truly at `+0x20` in the instance the ctor operates on (we verified via `dt SwgClient_r!ShaderImplementationPassPixelShaderProgram` → `+0x20`)? Any chance of a different layout for a derived/aliased type, or `createPixelShaderProgramData` storing through a copy?
4. **Why are the first ~9 programs (vertex_color, 3d_vertex_color, … a_modulate) in normal `0x0a` heap, but `2d_texture` onward in the `0x2124–0x2125` arena?** What allocator/region is that, and does crossing into it correlate with the corruption? (PPEM/`2d_texture` is the first arena program.)

## Proposed fix (challenge it)
A vtable-validity guard in `Direct3d11_StaticShaderData::apply` (+ construct snapshot): before the Plan-17-03 reflection/material loop, require `*(void**)m_graphicsData == &Direct3d11_PixelShaderProgramData::vftable`; if not a live PS-data, skip the loop and fall back (D3D9's `apply` never runs that loop — it delegates to `Direct3d9_ShaderImplementationData::apply`). It's reversible and D3D9-parity. **Is this the right fix, or does it mask a real engine corruption that will bite elsewhere (e.g. every arena-allocated post-FX/UI shader has a dangling `m_graphicsData`, so rendering is silently broken even if it stops crashing)?** What single experiment would you run next to find the *trigger* (it evades dtor/remove/slot-write watches)?

## Key file:lines
- `Direct3d11_StaticShaderData.cpp:857` (crash), `:782-789` (live-deref), `:459-466` (construct snapshot+slot)
- `ShaderImplementation.cpp:2754-2776` (Program ctor), `:2780-2793` (dtor), `:2804-2822` (fetch/release), `:2826-2863` (reload)
- `Direct3d11.cpp:666-669` (PS-data factory), `:267-269` (flushResources no-op)
- `PostProcessingEffectsManager.cpp:144,250-252` (ms_copyShader = 2d_texture)
- `Direct3d11_PixelShaderProgramData.cpp:52` (ms_memoryBlockManager), `install():1022`, `remove():1112`

## Deliverable
The most likely explanation for the paradox (slot non-NULL garbage at ctor + never written + valid PS-data created elsewhere + nothing freed via dtor), the file:line evidence, whether the vtable guard is correct or a band-aid over a broader corruption, and the ONE next experiment to pin the trigger. Call out every assumption of ours you think is wrong.
