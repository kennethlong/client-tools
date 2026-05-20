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
TBD — to land after Kenny smoke verdict.

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
