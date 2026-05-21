# Plan 11-09 Iteration Log — D3D11 Shader-Binding Arc

Per-iteration 6-field log (Date / Symptom / Hypothesis / Investigation / Root cause / Fix / Verification + Commit + Awaiting block). Mirrors Plan 11-07 + Plan 11-08 cadence.

---

## Iter-1: setStaticShader + StaticShaderData::apply per-pass VS + PS extraction

**Date:** 2026-05-20 (evening session, post-nap; followed morning's session handoff at `a3b5705dc`)

### Symptom (carried from Plan 11-08 close diagnostic)
Plan 11-08 closed at structural-prerequisites-complete milestone after 9 iterations. Post-Task-3b smoke captured a clean `stage/d3d11-debug.log` (64,450 INFO-severity messages, 0 ERROR, 0 WARNING) — but **visible geometry remained absent**. Diagnostic root cause locked:

- `Direct3d11_StateCache::setStaticShader` was a documented no-op (`StateCache.cpp:912`).
- `Direct3d11_StaticShaderData::apply` was Plan 11-07 Iter-1 minimum-viable (`StaticShaderData.cpp:157`), returning `false` unconditionally.
- `ms_currentVSData` stayed null per draw → `resolveShaders` short-circuited → `++ms_skippedDrawsNullVS` → **zero draws reached the GPU.**

CODEX post-implementation peer-review confirmed the diagnosis and **reframed the Iter-1 acceptance bar as "draw activity observable"** (CreateInputLayout count + IASet*/Draw* InfoQueue traffic + `ms_skippedDrawsNullVS` counter behavior) NOT "geometry visible" — geometry visibility is blocked by a separate PS NULL issue scheduled for Iter-2's magenta fallback PS.

### Hypothesis
Wire the per-pass VS + PS pointer extraction in `Direct3d11_StaticShaderData::construct()` + `Direct3d11_StaticShaderData::apply()`, and replace `Direct3d11_StateCache::setStaticShader`'s no-op body with the D3D9-pattern port. Expected consequences:

- `ms_currentVSData` receives a non-null pointer per draw (when the StaticShader has a valid VS for the requested pass).
- `resolveShaders` reaches `Direct3d11_VertexDeclarationMap::getOrCreate`.
- ≥1 `CreateInputLayout` call per session lands in `stage/d3d11-debug.log`.
- `IASet*` / `Draw*` / `DrawIndexed` InfoQueue traffic appears.
- `ms_skippedDrawsNullVS` stops climbing once a valid VS is bound.

Geometry visibility NOT expected at Iter-1 close — Iter-2's territory.

### Investigation

**Pre-implementation CODEX consult** (file: `.planning/phases/11-d3d11-renderer-plugin/11-09-CODEX-CONSULT-friend-additions.md`).

PLAN said "add 2 friend declarations to `ShaderImplementation.h:41-44`." But the canonical D3D9 traversal at `Direct3d9_StaticShaderData.cpp:988-992` touches 3 private fields (`m_effect`, `m_implementation`, `m_pass`) and D3D9 has 3 friend declarations across 3 headers (`StaticShaderTemplate.h:28`, `ShaderEffect.h:25`, `ShaderImplementation.h:44`). Asked CODEX whether the D3D11 mirror needs 2 friends, 3, or 4.

CODEX returned **Hypothesis A** — PLAN is correct; the public-API path is `shader.getStaticShaderTemplate().getShaderEffect().getActiveShaderImplementation()`, which CODEX confirmed by citing:
- `StaticShader.h:56` — public `getStaticShaderTemplate()`
- `StaticShaderTemplate.h:103` — public `getShaderEffect() const`
- `StaticShaderTemplate.cpp:189` — implementation returns `*m_effect`
- `ShaderEffect.h:40` — public `getActiveShaderImplementation()`
- `ShaderImplementation.h:129` — `m_pass` private (the one private field that still needs the friend)

I missed the `StaticShaderTemplate.h:103` public getter in my own earlier grep (I had the line in my output but didn't process it; CODEX caught it).

**First MSBuild attempt** (CODEX Hypothesis A; public-getter chain in ctor init list):
1. 1 compile error: `StaticShader::m_graphicsData private`. The setStaticShader port casts `shader.m_graphicsData` to `Direct3d11_StaticShaderData const *`, but `StaticShader::m_graphicsData` is private and `Direct3d11_StateCache` was not a friend. (CODEX's response didn't cover this access; PLAN didn't either. D3D9 solves via `friend class Direct3d9;` at `StaticShader.h:34`.)
2. 2 link errors (after adding `friend class Direct3d11_StateCache;` to StaticShader.h): **LNK2019 unresolved external** for `StaticShaderTemplate::getShaderEffect()` and `ShaderEffect::getActiveShaderImplementation()`.

**Link-error root cause:** plugin DLLs (`gl05_d.dll` / `gl11_d.dll`) cannot call non-inline non-DLLEXPORT member functions of clientGraphics classes — they can only access fields via friend, call inline functions defined in headers, or call Gl_api slot function pointers. D3D9 plugin doesn't hit this because it uses direct field access (`m_effect->m_implementation`), not the public getters. CODEX's public-API recommendation was correct in principle (minimize private surface) but **wrong on plugin DLL linkage constraints** — there is no equivalent linkage path for the public getter chain.

**Decision:** abandon Hypothesis A; mirror D3D9 verbatim via direct field access. Hypothesis C (originally rejected based on CODEX's Hypothesis A endorsement) becomes correct.

### Root cause (two-part)
1. `Direct3d11_StateCache::setStaticShader` was a documented no-op (Plan 11-06 design that turned out to block visible geometry per Plan 11-08 close diagnostic).
2. `Direct3d11_StaticShaderData::apply` returned `false` unconditionally and didn't extract per-pass VS/PS pointers (Plan 11-07 Iter-1 minimum-viable; documented as "lands when smoke surfaces a specific symptom" — the symptom is now landed in Plan 11-08's close diagnostic).

### Fix

**Engine-side friend declarations (5 lines across 4 headers; final shape, Hypothesis C mirroring D3D9):**

| File | Line | Friend added | Reason |
|---|---|---|---|
| `clientGraphics/src/shared/StaticShader.h` | 35 | `friend class Direct3d11_StateCache;` | `m_graphicsData` cast in `setStaticShader` |
| `clientGraphics/src/shared/StaticShaderTemplate.h` | 29 | `friend class Direct3d11_StaticShaderData;` | `m_effect` access in ctor |
| `clientGraphics/src/shared/ShaderEffect.h` | 26 | `friend class Direct3d11_StaticShaderData;` | `m_implementation` access in ctor |
| `clientGraphics/src/shared/ShaderImplementation.h` | 45-46 | `friend class Direct3d11_ShaderImplementationData;` + `friend class Direct3d11_StaticShaderData;` | `m_pass` access in construct |

PLAN deviation from "2 lines on `ShaderImplementation.h`" guidance: **+3 friend lines across +3 headers** (StaticShader + StaticShaderTemplate + ShaderEffect). Rationale documented in the CODEX consult file (link-error discovery section).

**Plugin-side code changes (single iteration commit):**

`Direct3d11_StaticShaderData.h`:
- Added forward decls for `Direct3d11_VertexShaderData` + `Direct3d11_PixelShaderProgramData`.
- Added `#include <vector>`.
- Added 2 new private members: `std::vector<Direct3d11_VertexShaderData const *> m_passVS;` + `std::vector<Direct3d11_PixelShaderProgramData const *> m_passPS;`. Non-owning (D3D9 mirror; CODEX Q4 confirmed lifetime safety).

`Direct3d11_StaticShaderData.cpp`:
- Added `#include "Direct3d11_StateCache.h"` + `#include "clientGraphics/ShaderEffect.h"`.
- Ctor init list populates `m_implementation` via D3D9 verbatim field access: `shader.getStaticShaderTemplate().m_effect->m_implementation`.
- `construct()` walks `m_implementation->m_pass[]` and populates `m_passVS` / `m_passPS` from:
  - VS: `pass->m_vertexShader->m_graphicsData` (single hop).
  - PS: `pass->m_pixelShader->m_program->m_graphicsData` (two hops — `pass.m_pixelShader` is `ShaderImplementationPassPixelShader *`, which holds `m_program` to `ShaderImplementationPassPixelShaderProgram *`, which holds the actual `m_graphicsData`).
- `apply()` bounds-checks `passNumber`, calls `Direct3d11_StateCache::setCurrentVSData()` + `setCurrentPSData()` on cache miss, returns `m_passVS[idx] != nullptr`. Cache-hit path skips the setters but still returns based on the cached VS pointer.

`Direct3d11_StateCache.h`:
- Added forward decls for `Direct3d11_VertexShaderData` + `Direct3d11_PixelShaderProgramData`.
- Declared 2 new public statics: `setCurrentVSData()` + `setCurrentPSData()`.

`Direct3d11_StateCache.cpp`:
- Added `#include "Direct3d11_StaticShaderData.h"`.
- Implemented `setCurrentVSData()` — assigns to namespace `ms_currentVSData`; trips `ms_geometryRebindNeeded = true` on change.
- Implemented `setCurrentPSData()` — assigns to namespace `ms_currentPSData` unconditionally.
- Replaced `setStaticShader` no-op body with D3D9-pattern port: `static_cast` `shader.m_graphicsData` → `Direct3d11_StaticShaderData const *`, `NOT_NULL`, `IGNORE_RETURN(d3dShader->apply(pass))`.

### Verification

**Pre-flight baseline (before any edit):**
- MSBuild Direct3d11 Debug EXIT=0 at HEAD = `a3b5705dc` (handoff commit).

**Post-edit build (4 attempts; final EXIT=0 across all 5 targets):**

| Attempt | What changed | Result |
|---|---|---|
| 1 | Hypothesis A (public-getter chain) + 2 friends on ShaderImplementation.h | 1 compile error (`StaticShader::m_graphicsData` private) |
| 2 | Added `Direct3d11_StateCache` friend to StaticShader.h | 2 link errors (LNK2019 for non-inline clientGraphics functions) |
| 3 | Switched ctor init list to D3D9 verbatim field access (`m_effect->m_implementation`); attempted StaticShaderTemplate.h friend addition but the Edit silently no-op'd (file not Read first) | 1 compile error (`StaticShaderTemplate::m_effect` private) |
| 4 | StaticShaderTemplate.h friend properly added after Read | **EXIT=0** across Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient |

**Build artifacts (post-attempt 4):**
- `compile\win32\Direct3d11\Debug\gl11_d.dll` → staged
- `compile\win32\Direct3d9\Debug\gl05_d.dll` → staged
- `compile\win32\Direct3d9_ffp\Debug\Direct3d9_ffp.dll` → built
- `compile\win32\Direct3d9_vsps\Debug\Direct3d9_vsps.dll` → built
- `compile\win32\SwgClient\Debug\SwgClient.exe` → staged

**Invariants preserved:**
- D-13 (ComPtr ownership only): no raw pointer ownership introduced; `m_passVS` / `m_passPS` are non-owning per CODEX Q4.
- D-04a (no D3D9-FFP-pattern code paths): preserved; new construct() handles VSPS-style passes only.
- D-05 (D3D9 plugin builds clean): preserved across all 3 D3D9 variants.

**MSVC warnings:** only the pre-existing C4459 ('E' shadow in `DirectXMathVector.inl`) and C4456 ('present' shadow in `Direct3d9.cpp`); both inherited from prior plans, no new warnings introduced.

**Build logs persisted:**
- `build-log-11-09-iter1-01.txt` (attempt 1)
- `build-log-11-09-iter1-02.txt` (attempt 2)
- `build-log-11-09-iter1-03.txt` (attempt 3)
- `build-log-11-09-iter1-04.txt` (attempt 4, success)

### Commit
`288b7b5f7` — `feat(11-09): wire setStaticShader + StaticShaderData::apply per-pass VS+PS extraction (Iter-1; CODEX-reframed acceptance bar 'draw activity observable')`. Pushed to `origin/koogie-msvc-cpp20-base` 2026-05-20 (post-nap session). 10 files changed, 499 insertions(+), 53 deletions(−).

### Awaiting Kenny smoke (Iter-1 verdict; CODEX-reframed acceptance bar)

**Smoke protocol:**
1. `Remove-Item D:\Code\swg-client-v2\stage\d3d11-debug.log -ErrorAction SilentlyContinue` — clean baseline
2. Verify `stage\client_d.cfg`: `[ClientGraphics] rasterMajor=11` + `[ClientGame/Bloom] enable=0`
3. Launch `D:\Code\swg-client-v2\stage\SwgClient_d.exe`
4. Run `D:\Code\swg-client-v2\tools\d3d11-smoke\show-window.ps1` if window stays hidden (Iter-17 carry-forward)
5. Run ≥60 sec; close cleanly

**Measurements (CODEX-reframed bar):**
```powershell
(Select-String -Path stage\d3d11-debug.log -Pattern 'CreateInputLayout').Count       # must be >= 1
(Select-String -Path stage\d3d11-debug.log -Pattern 'D3D11 \[ERROR\]').Count          # must be 0
(Select-String -Path stage\d3d11-debug.log -Pattern 'IASet|Draw|DrawIndexed').Count   # must be > 0
```

**Verdict pathways:**
- **GO** → CreateInputLayout ≥1 + ERROR=0 + IASet/Draw > 0 + process responsive ≥60 sec + dark-blue clear preserved → **commit Iter-1**, advance to Iter-2 (magenta fallback PS).
- **REVIEW** → Build OK but smoke surfaces unexpected behavior (crash, FATAL, new ERROR-severity D3D11 messages, missing dark-blue clear, IASet/Draw still 0). Capture `d3d11-debug.log`, post analysis here; iterate within Iter-1 or open Iter-1.X tuning iteration.
- **REVERT** → BSOD or critical OS-level regression. `git restore` the working tree; record FAILURE per Plan 11-07 Iter-18 precedent; no commit. (Plan 11-08 Task-3b's full-fill cbuffer discipline + first-draw race guard means a BSOD here would indicate a problem in the new VS/PS extraction path rather than the cbuffer wiring — likely a stale pointer or uninitialized vector.)

**Expected outcome:** GO. Geometry remains invisible (Iter-2's territory) but draw activity is observable in the log.

---

## Iter-2: Direct3d11_PixelShaderProgramData magenta fallback PS so geometry surfaces visibly

**Date:** 2026-05-20 (same evening session as Iter-1, continuing post-Iter-1 push at `cf40c74a3` → `71a9869f0`)

### Symptom (predicted from CODEX cascade map)
Iter-1 lands the shader-binding chain — `setStaticShader` ports, `StaticShaderData::apply` extracts per-pass VS+PS, `ms_currentVSData` receives non-null pointers, `resolveShaders` reaches `VertexDeclarationMap::getOrCreate`, draws submit to the GPU with input layouts. But the asset PS pointer (`ms_currentPSData->getPixelShader()`) is **always null for stock assets** per Plan 11-05's PEXE caveat — D3D11 `CreatePixelShader` rejected the D3D9-era pre-compiled PS bytecode at asset-load time, so every `Direct3d11_PixelShaderProgramData::m_d3dPS` stays empty. With no PS bound, draws rasterize but produce no fragment writes; geometry is invisible.

CODEX cascade map item #1 (locked in the Plan 11-08 post-implementation consult): "Once VS binds and draws fire, PS NULL still blocks color output (D3D11 has no FFP pass-through; needs debug/fallback PS)."

### Hypothesis
Compile an install-time **magenta pass-through PS** as a singleton on `Direct3d11_PixelShaderProgramData` and have `applyPreDrawState` bind it whenever the active StaticShader's real PS pointer is null. Magenta = debug-visible "Phase 12 asset re-author owes us a real PS" signal that still surfaces geometry. This addresses cascade item #1 and reaches the Plan 11-09 EXIT MILESTONE: **visible geometry on top of the Plan 11-07 dark-blue clear in at least one scene.**

### Investigation
**Pre-edit bit-rot check on `compilePixelShaderFromHlsl` helper** (`Direct3d11_PixelShaderProgramData.cpp:73-248`):
- Helper has been maintained in lockstep with the VS site across Plans 11-05 + 11-07 + 11-08. Latest touches: Iter-1.5 ROW_MAJOR retrofit (D3DCOMPILE_PACK_MATRIX_ROW_MAJOR flag, REWRITE_VERSION bumped 13→14), Iter-3 profile switch (`ps_4_0_level_9_3`), Iter-5 BACKWARDS_COMPATIBILITY flag.
- Helper internals: applies `Direct3d11_HlslRewrite::applyToMainSource` (no-op for minimal source with no `#include`/`: register(...)`), calls `Direct3d11_ShaderCache::tryLoad` + `D3DCompile` + `Direct3d11_Device::getDevice()->CreatePixelShader`, FATALs on D3DCompile failure with the error blob inline.
- Not bit-rotted. First-real-caller call site is install() per PLAN.

**PS input-signature consideration:**
- D3D11 spec: PS input must be a subset of VS output. `SV_POSITION` is always present (rasterizer mandate). Declaring only `float4 pos : SV_POSITION` is safe regardless of what the engine's VS outputs.
- No CODEX consult needed; standard minimal-PS pattern.

### Root cause
Plan 11-05 documented PEXE caveat: `ShaderImplementationPassPixelShaderProgram::m_exe` is pre-compiled D3D9 PS bytecode; `Direct3d11_PixelShaderProgramData::Direct3d11_PixelShaderProgramData` ctor detects this (line 338-342) and leaves `m_d3dPS = nullptr`. Iter-1's `applyPreDrawState` then hit the "leave previous PS bound" branch (StateCache.cpp:464-465 pre-edit) which produced no fragment writes.

### Fix

**`Direct3d11_PixelShaderProgramData.h`:**
- Added public static `getFallbackPS()` accessor + private static `ms_fallbackPS` ComPtr member.
- Inline `getFallbackPS()` definition at end-of-header (matches `getPixelShader()` inline pattern).

**`Direct3d11_PixelShaderProgramData.cpp`:**
- Added static `ms_fallbackPS` definition (ComPtr<ID3D11PixelShader>, default-constructed empty).
- Dropped `[[maybe_unused]]` from `compilePixelShaderFromHlsl` helper signature — now has a real caller.
- `install()` compiles a minimal inline HLSL ("float4 main(float4 pos : SV_POSITION) : SV_TARGET { return float4(1.0f, 0.0f, 1.0f, 1.0f); }") via the helper into `ms_fallbackPS`. Double-FATAL on compile failure: once on helper return value, once on empty-ComPtr post-success (defensive).
- `remove()` releases `ms_fallbackPS` via `.Reset()` before deleting the memory block manager.

**`Direct3d11_StateCache.cpp::applyPreDrawState` (PS-bind block):**
- Replaced single-if/leave-previous-bound with if/else-if:
  - First branch: `ms_currentPSData` has real PS → `PSSetShader(real)`.
  - Second branch: `Direct3d11_PixelShaderProgramData::getFallbackPS()` returns non-null → `PSSetShader(fallback)` (the magenta singleton).
  - Defensive else: leave previous PS bound (should never fire post-Iter-2 because `install()` FATALs on fallback compile failure).

### Verification

**MSBuild EXIT=0** across Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient — one attempt, clean.

**Build artifacts:**
- `compile\win32\Direct3d11\Debug\gl11_d.dll` → staged
- `compile\win32\Direct3d9\Debug\gl05_d.dll` → staged
- `compile\win32\Direct3d9_ffp\Debug\Direct3d9_ffp.dll` → built
- `compile\win32\Direct3d9_vsps\Debug\Direct3d9_vsps.dll` → built
- `compile\win32\SwgClient\Debug\SwgClient.exe` → staged

**Invariants preserved:** D-13 (ComPtr ownership only — `ms_fallbackPS` is `ComPtr<ID3D11PixelShader>`), D-04a (no FFP code paths), D-05 (D3D9 plugin builds clean).

**MSVC warnings:** unchanged from Iter-1 — pre-existing C4459 + C4456 + MSB8012 (SwgClient TargetName/OutputFile mismatch from Plan 11-06 era). No new noise.

**Build log:** `build-log-11-09-iter2-01.txt`.

### Commit
TBD — to land after Kenny smoke verdict.

### Awaiting Kenny smoke (Iter-2 verdict; **Plan 11-09 EXIT MILESTONE candidate**)

**Smoke protocol:**
1. `Remove-Item D:\Code\swg-client-v2\stage\d3d11-debug.log -ErrorAction SilentlyContinue` — clean baseline
2. Verify `stage\client_d.cfg`: `[ClientGraphics] rasterMajor=11` + `[ClientGame/Bloom] enable=0`
3. Launch `D:\Code\swg-client-v2\stage\SwgClient_d.exe`
4. Run `D:\Code\swg-client-v2\tools\d3d11-smoke\show-window.ps1` if window stays hidden
5. Run ≥60 sec; close cleanly

**Measurements:**
```powershell
(Select-String -Path stage\d3d11-debug.log -Pattern 'CreatePixelShader').Count       # must be >= 1 (the fallback compile)
(Select-String -Path stage\d3d11-debug.log -Pattern 'CreateInputLayout').Count       # must be >= 1 (Iter-1 carry-forward)
(Select-String -Path stage\d3d11-debug.log -Pattern 'D3D11 \[ERROR\]').Count          # must be 0
(Select-String -Path stage\d3d11-debug.log -Pattern 'D3D11 \[WARNING\]').Count        # tolerable; triage if non-zero
```

**Visible expectations:**
- Dark-blue clear preserved (Plan 11-07 milestone)
- **Magenta geometry visible on top of the clear** — this is the Plan 11-09 EXIT MILESTONE
- UI still alive (cursor capture, login screen audio/hover SFX)
- Process responsive ≥60 sec

**Verdict pathways:**
- **GO** → magenta geometry visible + 0 ERROR + ≥60 sec responsive → **commit Iter-2**, declare Plan 11-09 minimum-viable EXIT MILESTONE reached. Next decisions:
  - Continue with Iter-3+ asset cascade items within Plan 11-09 (per-pass material/texFactor/etc., VS texcoord-set variants, IL signature mismatches), OR
  - Close Plan 11-09 at minimum-viable milestone and open Plan 11-10 (DPVS re-measurement). Strategic decision left to Kenny.
- **REVIEW <details>** → build OK but smoke surfaces unexpected behavior. Likely cases: (a) no magenta visible (fallback bind didn't fire — investigate setCurrentPSData wiring), (b) magenta visible but D3D11 ERRORs appear (PS signature mismatch — investigate), (c) magenta visible but at wrong positions (WVP matrix problem from Plan 11-08 surfacing). Iterate within Iter-2 or open Iter-2.X.
- **REVERT <reason>** → BSOD or critical regression. `git restore` per Plan 11-07 Iter-18 precedent; no commit. (Unlikely — Iter-2 doesn't add new cbuffer/RTV/IL surface; just a single ID3D11PixelShader bind.)

**Expected outcome:** GO. Iter-2 is mechanical work on a path Iter-1 proved out.

---

## Iter-2 close-out: 7-sub-iteration CODEX-driven arc surfaced 4 architectural bugs

**Date:** 2026-05-21 (next-morning continuation of the 2026-05-20 evening session)

### Reality check

Base Iter-2 built clean and the fallback bind branch fired per probes, **but no magenta was visible**. The "Iter-2 is mechanical work" expected outcome was wrong — visible geometry was blocked by 3 additional architectural bugs upstream of the PS bind. CODEX-driven diagnostic narrowing across 6 consult rounds + 7 sub-iterations (2.1 → 2.7g) surfaced and fixed all of them.

### Sub-iteration log

| Sub-iter | What landed | Status at close-out |
|---|---|---|
| Iter-2 base | Magenta fallback PS install + applyPreDrawState fallback bind branch | KEEP (real fix) |
| Iter-2.1 | install / compile / fallback-bind / real-bind first-fire probes | STRIPPED |
| Iter-2.2 | Pipeline statistics query (Begin/End/GetData around first 10 draws) — proved CPrimitives=10 but PSInvocations=0 → pre-PS rejection, not post-PS | STRIPPED |
| Iter-2.3 | DepthEnable=FALSE diagnostic; magenta still missing → depth ELIMINATED as suspect | reverted in 2.4 |
| Iter-2.4 | Extended state probes at first-fallback bind (viewport / scissor / IL / topology / VBFormat) — proved viewport/IL/topology all sane | STRIPPED |
| Iter-2.5 | WVP matrix dump in composeSlot0Shadow — proved matrices look fine | STRIPPED |
| Iter-2.6 | First-fire probes on Plan 11-08 matrix setters — proved engine NEVER calls them for UI draws; UI VS reads c9 viewportData, not c0..c3 WVP | STRIPPED |
| Iter-2.7a (Fix C; CODEX Round 3) | namespace-scope `s_slot0Shadow` + dirty flag + lazy `flushSlot0IfDirty()` + `setVSConstants(register, data, count)` public thunk | KEEP (real fix) |
| Iter-2.7b (CODEX Round 4) | c9 `VSCR_viewportData` write in `setViewport` via D3D9's exact `{ 2/w, -2/h, -1-xOffset, 1+yOffset }` formula — **THE breakthrough; PSInvocations 0 → 3.9M** | KEEP (real fix) |
| Iter-2.7c (CODEX Round 5) | OMGetRenderTargets probe at first-fallback (boundRTV == backbufferRTV → ruled out off-screen RT hypothesis) + `Direct3d11_Device::getBackBufferRTV()` public accessor | probe STRIPPED; accessor KEEP |
| Iter-2.7d | beginScene / clearViewport / endScene / applyPreDrawState event-order probes — proved engine calls clearViewport AFTER UI draws within same frame (clear-after-draw smoking gun) | STRIPPED |
| Iter-2.7e | EXPERIMENTAL: disable color-clear branch entirely → magenta visible for first time (confirms clear-after-draw was wiping the just-drawn UI) | superseded by 2.7f |
| Iter-2.7f (CODEX Round 6) | `clearViewport` clears BOUND RTV with engine's `colorValue` (ARGB→float4); primary-backbuffer escape hatch defers clear-after-draw to next beginScene when `s_frameHasDraws` is true; `Direct3d11_Device::setFrameHasDrawActivity()` called from applyPreDrawState success path | KEEP (real fix) |
| Iter-2.7g | Strip all Iter-2.X diagnostic probes | THIS commit |

### 4 architectural bugs surfaced + fixed

1. **PS NULL silent invisibility** (Plan 11-05 PEXE caveat). `Direct3d11_PixelShaderProgramData::install()` now compiles a minimal magenta SV_TARGET pass-through PS via the previously-`[[maybe_unused]]` `compilePixelShaderFromHlsl` helper. `applyPreDrawState` falls back to `ms_fallbackPS` when `ms_currentPSData` lacks a real PS. Visible magenta = debug-only Phase 12 asset re-author owes us real shaders.

2. **Plan 11-05 dead-static user-constants** (CODEX Round 3 / Fix C). `setVertexShaderUserConstants_impl` previously stored to a function-local-static shadow that nothing read. Lifted to namespace-scope `Direct3d11_VertexSlot0CB s_slot0Shadow` + dirty flag + lazy `flushSlot0IfDirty()` called once-per-draw at `applyPreDrawState` top. Engine pushes route via `Direct3d11_StateCache::setVSConstants(register, data, count)` at register `VCSR_userConstant0 + index` (= 52 + index, verified against `Direct3d9_VertexShaderConstantRegisters.h:37`).

3. **Missing VSCR_viewportData at c9** (CODEX Round 4 breakthrough). `Direct3d11_StateCache::setViewport` now writes c9 viewport scale/bias via D3D9's exact formula `{ 2/width, -2/height, -1-xOffset, 1+yOffset }`. Pre-fix: c9 stayed at install-time zero, UI VS multiplied screen-space verts by zero → CPrimitives=10 but PSInvocations=0 (degenerate triangles). Post-fix: 3.9M PSInvocations.

4. **clearViewport-after-draw under D3D11 flip-model** (CODEX Round 6 architecture). `clearViewport` previously hardcoded dark-blue clear on `ms_backBufferRTV` unconditionally, ignoring engine's `colorValue` and the currently-bound RTV. Now clears bound RTV with engine's colorValue (ARGB→float4) AND defers primary-backbuffer clear to next beginScene when `s_frameHasDraws` is true. Mirrors D3D9 Clear semantics while accommodating flip-model's wipe-on-clear hazard.

### Close-out smoke (2026-05-21, post-Iter-2.7g probe strip)

- MSBuild EXIT=0 across Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient
- D-05 invariant preserved (D3D9 plugin builds clean)
- D-13 invariant preserved (ComPtr ownership only; namespace shadow is value-typed; no raw GPU resource pointers introduced)
- Visible: **half-magenta / half-black on the login screen**, sustained ≥60 sec, no FATAL / no BSOD
- `iter2-probe.txt` NOT created post-strip → all probe sites stripped clean

### Plan 11-09 EXIT MILESTONE: REACHED

Visible magenta geometry on the engine-intended clear. Login-screen UI quads render via the fallback PS over half-screen coverage (full coverage is Plan 11-10 quad-list-expansion work).

### CODEX consults preserved (7 files in this directory)

1. `11-09-CODEX-CONSULT-friend-additions.md` — Round 1 + Iter-1 close addendum (friend set decision)
2. `11-09-CODEX-CONSULT-iter2-magenta-missing.md` — Round 1 (magenta-missing initial diagnosis)
3. `11-09-CODEX-CONSULT-iter2-pipeline-stats.md` — Round 2 (pipeline-stats query design + narrowing)
4. `11-09-CODEX-CONSULT-iter2-dead-user-constants.md` — Round 3 (Plan 11-05 dead user-constants finding → Fix C)
5. `11-09-CODEX-CONSULT-iter2-no-constants-push.md` — Round 4 (setVertexShaderUserConstants also never called → c9 viewport breakthrough)
6. `11-09-CODEX-CONSULT-iter2-post-ps-rejection.md` — Round 5 (post-PS rejection ranking → OMGetRenderTargets diagnostic)
7. `11-09-CODEX-CONSULT-iter2-clearview-architecture.md` — Round 6 (clearViewport architecture + half-quad → Plan 11-10)

### Commit (close-out)

TBD — atomic Plan 11-09 close commit (to be filled by docs touch-up after the close commit lands; mirrors Plan 11-08 / Plan 11-09 Iter-1 pattern `288b7b5f7` → `cf40c74a3`).

### Plan 11-10 scope

Half-quad rendering (engine submits 4-vert `drawTriangleList` expecting 1 triangle by D3D9 convention; full quad coverage requires either `CuiLayerRenderer` / `drawQuadList` wiring per `Direct3d9.cpp:4277` reusable-index-buffer pattern, OR per-pass DSS/BS overrides from `Direct3d11_ShaderImplementationData::apply`, depending on which engine path surfaces the next visual symptom).

**PS user-constants are still broken** (Plan 11-05 dead-static bug for `setPixelShaderUserConstants_impl`). Plan 11-09 fixed only VS user-constants. Plan 11-10 should mirror Fix C for PS slot 0 (which currently doesn't have an equivalent `Direct3d11_PixelSlot0CB` struct).

`Direct3d11_StateCache::drawQuadList` is still STUB. Plan 11-10 territory.

---

