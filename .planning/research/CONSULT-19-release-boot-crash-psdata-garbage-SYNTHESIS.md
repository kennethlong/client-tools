# Synthesis: D3D11 release boot-crash — root cause (CODEX + Cursor + dump-verified)

**Date:** 2026-06-02. **Inputs:** `…-codex.out`, `…-cursor.out`, full dump `stage/release-gl11-crash.dmp`, gl11 symbols.
**Method:** both AIs reviewed independently; I then verified their falsifiable claims directly in the dump (consensus≠correctness).

## ⚠️ RUNTIME-VERIFIED UPDATE 2026-06-02 (supersedes the "freed/reused" mechanism below)
A logging-breakpoint run (`dbg-d3d11-progfree.cmd`: bps on `ShaderImplementationPassPixelShaderProgram::{~dtor,release,asynchronousLoaderRelease}` + `Direct3d11_PixelShaderProgramData::~dtor`, interactive so DirectInput init succeeds) booted all the way to the crash and fired **ZERO times**. **Nothing is freed during boot** → there is **NO use-after-free**. Both consults' leading "freed MemoryBlockManager block reused as string arena" mechanism is **FALSIFIED**.

Clean chain-walk in `release-gl11-crash.dmp` (single dump, ASLR-consistent, vtable identified symbolically):
- `StaticShaderData(0x0a9705e0)` → `m_shader 0x0a96eeb0` (StaticShader, coherent, round-trips: its `m_graphicsData` == the wrapper) → `m_implementation 0x2124fb70` (ShaderImplementation, **coherent**: `m_users=1`, valid `m_graphicsData`/`m_pass`) → `m_pass` vec → `pass[0] 0x2124fbb0` → `m_pixelShader 0x0a0df4a0` → **`m_program 0x21250010`**, `m_fileName = "pixel_program/2d_texture.psh"`, `m_referenceCount = 1` (ALIVE).
- The `0x2124`/`0x2125` addresses are a **legit shader-object pool arena**, not garbage — everything above `m_graphicsData` is coherent.
- `program->m_graphicsData = 0x21250000`; its vtable `0x2124ffd0` ≠ the real `gl11_r!Direct3d11_PixelShaderProgramData::vftable 0x6106b9f0`. `0x21250000` is unrelated memory (PersistentCrcString-ish), **never a gl11 PS-data**.

**Corrected root cause: the alive, valid `2d_texture.psh` Program's `m_graphicsData` was NEVER assigned a real gl11 `Direct3d11_PixelShaderProgramData` — it holds a non-null garbage value (and `snapshot==live`, so it was already garbage at `construct()`). This is a CREATION/assignment gap, not a free.** Likely: `Graphics::createPixelShaderProgramData` was never called for this PPEM program (capability gate at `ShaderImplementation.cpp:2765-2770`?) or ran against a stale device/plugin-init order, leaving `m_graphicsData` a leftover non-null value.

**Refined fix:**
- **Immediate, principled guard (now justified by evidence):** in `Direct3d11_StaticShaderData::apply` (and the `construct()` snapshot), validate `*(void**)m_graphicsData == &Direct3d11_PixelShaderProgramData::vftable` before using it; if not a real PS-data, skip the Plan-17-03 reflection/material loop and fall back (D3D9 never runs that loop). This is a vtable check, not a magic-number reject.
- **Real fix (confirm first):** a CREATION watch — bp on `gl11_r!...createPixelShaderProgramData` (Direct3d11.cpp:666-669) + the Program ctor's `m_graphicsData =` assignment (ShaderImplementation.cpp:2765-2770), filtered to `2d_texture.psh` — to prove whether it's never called (creation gap → ensure it runs) vs called-then-stale. Then fix the gating/ordering so PPEM/UI programs get valid gl11 PS-data.

---

## Verified root cause (ORIGINAL — mechanism corrected by the UPDATE above; "not stale-cache UAF" still holds)
At the first `PostProcessingEffectsManager::postSceneRender` (boot, ~1s), `Direct3d11_StaticShaderData::apply` (pass 0) dereferences a per-pass pixel-shader pointer that is **garbage**, and crashes iterating `livePS->getReflectedCbufferLayouts()` (vector `begin=0x1`) at `Direct3d11_StaticShaderData.cpp:857`.

The garbage is **an interior pointer into a freed/reused engine `Program` object** — not a stale gl11 cache, not a recreated-PS-data UAF:

- `m_passPSSlot[0]` = `&program->m_graphicsData` = `0x21250030`. Engine `m_graphicsData` is at **`+0x20`** in `ShaderImplementationPassPixelShaderProgram` (verified via symbols) → the **Program base = `0x21250010`**.
- Program @ `0x21250010` is incoherent: `m_exe=0x21250680→0xffff0101`, `m_psrcLen=-707684993` → freed-and-reused memory (the `refcount=1` byte coincides with the bogus vector `begin=1`).
- `*slot` (`m_graphicsData`) = `0x21250000`. Its `+0x00` = `0x2124ffd0`, **not** gl11's real `Direct3d11_PixelShaderProgramData` vtable `0x6106b9f0` → **not a valid PS-data**.
- **snapshot `m_passPS[0]` == live `*slot` == `0x21250000`** → the value never changed; reading it "live" cannot help. The dangling pointer lives in the **engine** `Program`, which our slot points into.

## What both consults agreed on (and I confirmed)
1. **The falsified UAF story is dead.** `Direct3d11::flushResources` is a no-op (`Direct3d11.cpp:267-269`) — there is no D3D9-style device remove/restore that recreates `m_graphicsData` mid-boot. (Cursor, code-level.) The `StaticShaderData.{h,cpp}` comments asserting this are WRONG and must be rewritten.
2. **The crashing shader is NOT `ui_planet_sel_ordmantel_as8.sht`** — that's the "last loaded template" breadcrumb from `ShaderTemplateList::create()`. The real shader is PPEM's `ms_copyShader` = `shader/2d_texture.sht`, bound at `PostProcessingEffectsManager.cpp:144/250`. (Both.)
3. **PPEM correctly uses `StaticShaderData::construct/apply`** — same as D3D9. The exposure is the **Plan 17-03 reflection/material-upload loop** D3D11 added (`StaticShaderData.cpp:831-857`), which assumes `livePS` is a real PS-data. D3D9's `apply` delegates to `Direct3d9_ShaderImplementationData::apply(pass)` and runs no such loop. (Both.)
4. **The real fix is reference ownership**, not the live-deref. Caching `&program->m_graphicsData` is only safe if the wrapper keeps the `Program` (and the pass pixel-shader) alive. (Both — CODEX strongest: the live-slot change is conceptually *worse* because it dereferences a freed interior pointer.)

## Divergence, resolved by the dump
- Cursor: "`0x21250000` is a freed *PixelShaderProgramData* block." CODEX: "`0x21250000` is the freed *Program* object itself." **Both geometry guesses were slightly off** (`m_graphicsData` is at `+0x20`, so the Program is at `0x21250010`, not `0x21250000`). Net: the whole `0x21250000–0x21250680` region is **freed/reused program+string+freelist memory**; neither a valid Program nor a valid PS-data survives there. CODEX's *mechanism* (interior pointer into a dead Program) is the correct framing.

## Recommended fix (in priority order)
1. **Lifecycle / ownership (preferred, the real fix):** `Direct3d11_StaticShaderData` must hold a reference that keeps the engine objects whose interior it points into alive for the wrapper's lifetime — `fetch()` the `ShaderImplementationPassPixelShaderProgram` / pass pixel-shader in `construct()` and `release()` in the dtor / before `update()` rebuilds. Then both the cached slot and a live re-read are meaningful. (Both consults.)
2. **Principled validity guard (interim, NOT a magic-number reject):** before the material loop, require the factory's ownership invariant:
   - `engProgram->m_graphicsData == livePS` (engine slot agrees), AND
   - `livePS`'s vtable == `Direct3d11_PixelShaderProgramData::vftable` (a real object — directly testable; in this dump it's `0x2124ffd0 != 0x6106b9f0`), AND ideally
   - `((Direct3d11_PixelShaderProgramData*)livePS)->m_program == engProgram` (round-trip; needs a small `getProgram()` accessor, `m_program @ +0x04`).
   On failure: skip reflection/material upload, fall back to the per-VS / magenta PS path (mirrors D3D9 not running the loop), log once per `(engProgram,livePS)`.
3. **Rewrite the falsified comments** in `Direct3d11_StaticShaderData.{h,cpp}` (the device-restore UAF narrative).

## OPEN QUESTION (the "why", not fully answered by either)
Why is the PPEM `2d_texture` `Program` freed/reused while a `StaticShaderData` wrapper still references it during boot — in RELEASE but not DEBUG? Both consults say heap-timing (release reuses the freed block immediately; debug delays reuse → looks fine). The decisive next step (both agree): **break on `ShaderImplementationPassPixelShaderProgram::release()`/dtor (and `Program::release` refcount→0) for `2d_texture.psh` before the first `postSceneRender`** and find the unbalanced `release()` (candidate: `asynchronousLoaderRelease`, `ShaderImplementation.cpp:2697-2714`). If the program is genuinely never validly *created* in release instead, the ownership fetch won't help and we must ensure `createPixelShaderProgramData` runs — distinguish by checking, on the next dump, whether the program/PS-data are valid *before* any free.

## Cheap verification after a fix
Next full dump, same shader/pass: `pass->m_pixelShader->m_program` is a live `ShaderImplementationPassPixelShaderProgram` named `pixel_program/2d_texture.psh`; `m_graphicsData` → a live `Direct3d11_PixelShaderProgramData` (vtable `0x6106b9f0`), NOT back into the program block; PS-data `m_program (+0x04)` round-trips to the program; `getReflectedCbufferLayouts()` `{begin,end}` both null/empty or a valid bounded heap pair — never `begin=0x1`.
