# CODEX Consult — Plan 11-09 Iter-2 Magenta Geometry Not Surfacing

**Date:** 2026-05-20 (post-Iter-2 smoke)
**Status:** Iter-2 build clean; smoke shows Plan 11-07 dark-blue clear preserved (engine alive, 60+ sec runtime, no FATAL) but the magenta fallback-PS geometry that Iter-2 was supposed to surface is NOT visible.

---

## What Iter-2 ships (verified by binary string-grep on staged DLL)

`Direct3d11_PixelShaderProgramData::install()` compiles a minimal magenta pass-through PS at plugin install time and stores it in `ms_fallbackPS`:

```hlsl
// Plan 11-09 Iter-2 magenta fallback PS.
float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    return float4(1.0f, 0.0f, 1.0f, 1.0f);
}
```

Compile path: `compilePixelShaderFromHlsl` helper (Plan 11-05; first real caller is Iter-2) → optional shader-cache `tryLoad` → `Direct3d11_HlslRewrite::applyToMainSource` → `D3DCompile` (profile `ps_4_0_level_9_3`; flags D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR) → unconditional `Direct3d11_Device::getDevice()->CreatePixelShader` → returns true. `install()` FATALs if helper returns false OR if `ms_fallbackPS` is empty afterward.

`Direct3d11_StateCache::applyPreDrawState` PS-bind block (replaces Iter-1's "leave previous bound" no-op):

```cpp
ctx->VSSetShader(vs, nullptr, 0);
if (ms_currentPSData && ms_currentPSData->getPixelShader())
{
    ctx->PSSetShader(ms_currentPSData->getPixelShader(), nullptr, 0);
}
else if (ID3D11PixelShader *fallback = Direct3d11_PixelShaderProgramData::getFallbackPS())
{
    ctx->PSSetShader(fallback, nullptr, 0);
}
// else: leave previous PS bound (defensive; should never fire)
```

## Build state (verified)

- MSBuild EXIT=0 across all 5 targets (Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient).
- `stage/gl11_d.dll` (post-build copy) timestamp 15:22 today; size 1,503,744 bytes.
- Binary string-grep confirms `fallback_magenta.hlsl`, `float4(1.0f`, `fallback magenta`, `Iter-2` literals are present in the staged DLL.
- D-05 invariant preserved (D3D9 plugin builds clean).

## Smoke results (verified by InfoQueue file sink at `stage/d3d11-debug.log`)

| Metric | Value | Interpretation |
|---|---|---|
| Process runtime | 60+ sec, clean exit | Engine reached steady state; no FATAL |
| Visible state | Dark-blue clear visible; **no geometry on top** | Plan 11-07 milestone PRESERVED; Plan 11-09 EXIT MILESTONE NOT REACHED |
| `D3D11 [ERROR]` count | 0 | Clean run, no validation errors |
| `D3D11 [WARNING]` count | 0 | Same |
| `Create ID3D11VertexShader` | 2 | Engine baseline (install-time VS load) |
| `Create ID3D11InputLayout` | 1 | **Iter-1 chain working** — proves `applyPreDrawState::resolveShaders` reached `VertexDeclarationMap::getOrCreate` and a (VBFormat, VS) pair produced a layout |
| `Create ID3D11PixelShader` | 0 | **Inconclusive** — this count is 0 across all 4 sessions in the 82k-line log file, including baseline sessions BEFORE Iter-2. Plan 11-05 PEXE caveat ctor never calls CreatePixelShader (m_d3dPS stays null on D3D9-era bytecode). So 0 here doesn't prove Iter-2's install() did NOT call CreatePixelShader — it might mean the InfoQueue doesn't surface PS Create events, OR install() ran but compile failed silently somehow. |
| `IASet*` / `Draw*` events | 0 | InfoQueue doesn't appear to log per-frame state changes / draws — only object lifecycle (Create/Destroy). So 0 here is also inconclusive. |
| FATAL | None | Process exit clean; install() FATAL guards never fired |

## What we DON'T have (the diagnostic gap)

The engine's `DEBUG_REPORT_LOG_PRINT` routes via `Report.cpp` to stdout / `OutputDebugString` / stderr. When SwgClient_d.exe launches from explorer (no attached console, no stdout redirection in this env), those messages go nowhere visible. The only working sink is Plan 11-08 Iter-1.7's `stage/d3d11-debug.log` (the InfoQueue file sink), which captures **only** D3D11-runtime-emitted validation messages — not application-side `DEBUG_REPORT_LOG_PRINT` text.

Result: I cannot directly observe whether
- `Direct3d11_PixelShaderProgramData::install()` was actually entered
- `compilePixelShaderFromHlsl` returned true
- `ms_fallbackPS` is non-empty post-install
- `applyPreDrawState`'s fallback-bind branch ever fired

without adding instrumentation.

## Three hypotheses

**Hypothesis A — Compile failed silently.** Despite the FATAL guards in `install()`, somehow the fallback PS doesn't end up as a valid `ID3D11PixelShader`. The defensive `else` branch in `applyPreDrawState` leaves the previously-bound PS in place (which is null on first draw); pixels rasterize but produce no fragment writes. *Tension:* FATAL guards should make this loud, and process exited clean.

**Hypothesis B — Compile succeeded but bind never fires.** `ms_currentPSData->getPixelShader()` is somehow non-null for the drawing path, so the first `if` branch takes precedence and the fallback path is skipped. Geometry rasterizes with a stale / wrong PS. *Tension:* Plan 11-05 PEXE caveat says m_d3dPS is universally null for stock assets, so this should not happen — but maybe one specific shader's `Direct3d11_PixelShaderProgramData::Direct3d11_PixelShaderProgramData` ctor took an unexpected path (e.g. detected DXBC magic and tried CreatePixelShader on garbage that succeeded?).

**Hypothesis C — Compile + bind both fire, but draws are silently discarded due to PS input-signature mismatch.** The minimal fallback PS declares input `float4 pos : SV_POSITION` only. The engine's VS may output additional interpolants (COLOR, TEXCOORD0..7, etc.). D3D11's input-signature linkage rule: PS input is a *subset* of VS output (extras VS-side are allowed to be dropped). Standard mainstream understanding says SV_POSITION-only is safe. *Tension:* this is the spec rule from MSDN; if it's wrong, surface the corner case.

## Question for CODEX

> Given the verified evidence above, which hypothesis (A / B / C) is most likely? Are there other failure modes — D3D11-specific gotchas — I should consider for "PS bound, draws submitting, dark-blue clear preserved, geometry not surfacing, zero ERROR/WARNING from the debug layer"?
>
> Sub-questions:
>
> 1. **PS input-signature**: is a SV_POSITION-only PS truly safe across all VS output signatures, or does D3D11 emit a WARNING (which I'd see) / ERROR (would see) / silent discard (would NOT see)? Is there a per-signature compatibility check that quietly fails draws?
>
> 2. **Plan 11-05 PEXE caveat assumption**: Direct3d11_PixelShaderProgramData ctor's heuristic at line 320 (`looksLikeDxbc(exe)`) checks for 'DXBC' magic. If asset bytecode happens to start with that magic by coincidence (false positive), ctor takes the early-return path and leaves `m_d3dPS` null without calling CreatePixelShader. But if it's a TRUE D3D9 PEXE blob, ctor takes the line 340 path and also leaves `m_d3dPS` null. Either way, `getPixelShader()` returns null in the drawing path. Is this confirmed? Or could some shader have `m_d3dPS` non-null via a path I'm missing?
>
> 3. **Instrumentation idiom**: with the engine's `DEBUG_REPORT_LOG_PRINT` invisible at runtime and the InfoQueue file sink limited to D3D11-emitted messages, what's the cleanest D3D11-native idiom to log application-side runtime state? Options I'm considering:
>    - (a) `ID3D11InfoQueue::AddApplicationMessage` — routes through Plan 11-08 Iter-1.7's existing file sink.
>    - (b) Direct `std::fopen` to a separate diagnostic file (e.g. `stage/iter2-probe.txt`) — keeps probes out of the main debug log but adds a second file artifact.
>    - (c) Expose a public `Direct3d11_Device::getInfoQueue()` accessor and use `AddApplicationMessage` from anywhere in the plugin.
>
> 4. **Other "silently invisible geometry" failure modes in D3D11** beyond the three I enumerated? E.g. depth-stencil state rejecting all pixels, blend state with zero source alpha + alpha-blend enabled, rasterizer state with cull mode rejecting all triangles, viewport set to 0x0, render-target state mismatched with the bound PS, etc.

## What I'm doing while you consult

Shipping **Iter-2.1** — a tiny instrumentation iteration via option (b): `std::fopen("iter2-probe.txt", "a")` in `install()` entry/exit + `applyPreDrawState` first-fire of each PS branch. Re-smoke → grep `stage/iter2-probe.txt` → disambiguate A vs B vs C empirically.

If Iter-2.1 confirms C (compile + bind both fired, no magenta), your guidance on sub-question 1 (PS signature linkage) becomes the critical input for Iter-2.2.

---

## ADDENDUM — Iter-2.1 probe output (2026-05-20 same session)

Iter-2.1 instrumentation built clean, smoked, probe file captured:

```
[Iter-2.1 probe] Direct3d11_PixelShaderProgramData::install() entered -- gl11_d.dll has Iter-2 code
[Iter-2.1 probe] compilePixelShaderFromHlsl returned: true
[Iter-2.1 probe] ms_fallbackPS pointer: 17530BBC
[Iter-2.1 probe] applyPreDrawState first FALLBACK PS bind: fallback=17530BBC, drawCallCount=0, ms_currentPSData=12EBC9B8, ms_currentPSData->getPixelShader()=00000000
```

**Hypotheses A and B are eliminated:**
- A (compile failed silently): refuted — compile returned true, ms_fallbackPS is a valid non-null pointer (0x17530BBC).
- B (bind never fires): refuted — `applyPreDrawState first FALLBACK PS bind` probe fired with `fallback=0x17530BBC` (matches install-time pointer exactly, so we're binding the right PS) and `ms_currentPSData->getPixelShader()=NULL` (real PS is null as Plan 11-05 PEXE caveat predicts).

**Hypothesis C confirmed:** Iter-2's code path works end-to-end. Magenta still not visible. The remaining failure must be in the **"silently invisible D3D11 geometry"** space — the bound fallback PS is rasterizing nothing visible despite being bound, OR draws are being culled/discarded by some pipeline state OR the VS is outputting positions that produce degenerate geometry.

`drawCallCount=0` at first-fire means the fallback was bound on the very first draw attempt of the session (counter only increments on successful applyPreDrawState completion); subsequent draws would have re-bound the same fallback (no probe — first-fire static bool sentinel suppresses), so we know it's not a one-shot issue.

## Refined question for CODEX (post-probe)

> Iter-2.1 probe data eliminates compile-failure and bind-skip hypotheses. The fallback PS (0x17530BBC, magenta SV_POSITION→SV_TARGET) IS being bound on the first draw, with a non-null ms_currentPSData whose real getPixelShader() is correctly null per Plan 11-05 PEXE caveat. Dark-blue clear (Plan 11-07 milestone) visible; no magenta visible; no D3D11 ERROR/WARNING. 60+ sec clean runtime.
>
> What's the most likely "silently invisible geometry" failure mode in this configuration? Prioritized list of suspects + the cheapest diagnostic for each:
>
> 1. **WVP matrix problem** — Plan 11-08 wired the per-object cbuffer composition (composeAndUploadSlot0 with WVP = P*V*W per CODEX Q1) but it was never validated by visible geometry (Plan 11-08 closed at "structural-prerequisites-complete"). If matrices produce degenerate positions (NaN, off-screen, transposed handedness), triangles cull silently. **Cheapest diagnostic?**
>
> 2. **Rasterizer cull mode** — D3D9 vs D3D11 default winding conventions. If engine vertex winding is CW and our RS desc uses BACK + CCW (or vice versa), all triangles silently reject. **Diagnostic: set D3D11_CULL_NONE temporarily and re-smoke?**
>
> 3. **PS input-signature with ps_4_0_level_9_3** — could feature-level-9 profile have stricter PS input requirements than the SM4+ baseline? Should the minimal fallback PS declare SV_POSITION + a dummy TEXCOORD0 to satisfy fl_9_3 linkage?
>
> 4. **Depth-stencil state** — dark-blue clear works, but per-pixel depth test could reject geometry if depth buffer isn't cleared correctly per frame, or if DSS desc rejects fragments at z=0/z=1 boundary.
>
> 5. **Other "silently invisible" D3D11 modes** I should consider? (Render target slot mismatch, viewport, blend state alpha=0, write-mask=0, etc.)
>
> Given that Plan 11-08 invested heavily in cbuffer wiring without visible verification, #1 is my prior. But the cheapest diagnostic is #2 (toggle cull mode) — confirm or eliminate before deeper investigation.

---
