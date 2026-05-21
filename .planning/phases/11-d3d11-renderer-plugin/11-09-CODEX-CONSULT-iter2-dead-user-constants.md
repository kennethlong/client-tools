# CODEX Consult Round 3 — Plan 11-05 User-Constants Stored to Dead Static

**Date:** 2026-05-20 (continuing session — same Iter-2 diagnostic arc)
**Predecessors:** Round 1 (magenta-missing) + Round 2 (pipeline-stats narrowing)
**Status:** Definitive bug located. Architectural decision required for the fix.

---

## CODEX Round 1+2 narrowing recap

Round 1 ranked WVP/clip/cull/depth/topology. Round 2's pipeline-statistics query proved:
- CPrimitives = CInvocations = 10 (clip + cull cleared)
- PSInvocations = 0 (zero fragments)
- Cull mode is sane, depth disabled didn't help, scissor disabled, viewport is full window 1024×768

You called WVP "my strongest suspect" and recommended sequencing through depth-off → quad-fix → matrix dump.

## Iter-2.5 + Iter-2.6 results

**Iter-2.5 matrix dump** at `composeAndUploadSlot0` showed:
- `s_anyMatrixWritten = 0` at the very first composeAndUploadSlot0 call (so the race guard fired with no prior setter activity)
- View / Proj / World / CameraPos all IDENTITY/zero (install-time defaults)
- Only ONE composeAndUploadSlot0 call in the entire 60+ sec session (no #2 or #3)

**Iter-2.6 first-fire setter probes:**
```
(no [Iter-2.6 probe] lines appeared in the output)
```

**Confirmation: ZERO of the three Plan 11-08 matrix setters (`setWorldToCameraTransform`, `setProjectionMatrix`, `setObjectToWorldTransformAndScale`) are EVER called by the engine in this session.**

Yet 11 successful draws occurred, with VS executing (VSInvocations=30), producing clip-space output (CInvocations=10) that survives clip+cull (CPrimitives=10) but is sub-pixel/degenerate (PSInvocations=0).

## The real bug

Engine uses `setVertexShaderUserConstants` (the D3D9 `SetVertexShaderConstantF` analog) — `ms_glApi.setVertexShaderUserConstants = setVertexShaderUserConstants_impl;` at `Direct3d11.cpp:664`. The implementation at lines 365-390:

```cpp
void setVertexShaderUserConstants_impl(int index, float c0, float c1, float c2, float c3)
{
    // ... comments claim "Plan 11-06's draw dispatch flushes via updateVS(1, ...)
    // + bindVS(1) at the right moment in the draw-call sequence" ...

    static Direct3d11_PerObjectCB s_perObjectShadow = {};   // FUNCTION-LOCAL STATIC
    if (index >= 0 && index < 8)
    {
        s_perObjectShadow.userConstants[index] = DirectX::XMFLOAT4(c0, c1, c2, c3);
    }
    // No upload anywhere.
}
```

**The static is function-local** — only the assignment site can access it. Plan 11-06's draw dispatch has no code to read this local-static. Grep confirms `s_perObjectShadow` is referenced only at lines 379 (declaration) + 382 (assignment). It's stored and forgotten.

The only `updateVS(1, ...)` call in the entire D3D11 plugin (`Direct3d11_ConstantBuffer.cpp:111`) is the install-time zero-fill from `primeDefaults`. Slot 1 stays zero for the lifetime of the process.

Same bug in `setPixelShaderUserConstants_impl` (lines 392-413) — `s_perMaterialShadow` is function-local-static, stored to but never uploaded.

## Resulting cbuffer state at draw time

| Slot | Plan owner | Real contents at draw time |
|---|---|---|
| VS slot 0 (1152 bytes, Plan 11-08 VertexSlot0CB) | Plan 11-08 matrix setters | Identity (primeDefaults + race guard; matrix setters never called) |
| VS slot 1 (Plan 11-05 PerObjectCB) | setVertexShaderUserConstants engine push | Zero (primeDefaults; engine push silently dropped) |
| PS slot 0 (fog cbuffer) | setFog | Zero (engine push dropped) |
| PS slot 2 (Plan 11-05 PerMaterialCB) | setPixelShaderUserConstants engine push | Zero (engine push silently dropped) |

VS reads c0..c71 from the bound cbuffer(s) → all identity/zero → output positions are input positions unchanged → after perspective divide (with whatever w the engine submitted), triangles collapse to sub-pixel/origin → CPrimitives counts them (homogeneous-space clip is lenient) but PSInvocations stays 0.

## Architectural decision required

The engine HLSL (after Plan 11-07's HlslRewrite Rule D wrapping) reads constants from cbuffer slot(s). I need to know which slot(s) the rewritten shader actually expects, then upload the engine's pushed user-constants there.

**Two plausible architectures:**

### Fix A: Restore Plan 11-05's design — make `setVertexShaderUserConstants_impl` actually upload to slot 1

Lift the shadow out of the function-local scope to namespace scope; on each setter call (or dirty-flagged before each draw), call `Direct3d11_ConstantBuffer::updateVS(1, &shadow, sizeof(shadow))`. Plan 11-07's HlslRewrite presumably maps the engine's `c0..cN` to cbuffer slot 1 (separate cbuffer declaration). This matches Plan 11-05's documented intent.

But then **Plan 11-08's entire matrix-shadow + composeAndUploadSlot0 machinery is dead code** — slot 0 stays at primeDefaults forever for the rest of the engine's life. Plan 11-08 retrofits were chasing a non-existent bug.

### Fix B: Engine pushes c0..cN go into slot 0 (Plan 11-08 layout)

Plan 11-07's HlslRewrite wraps everything into a single cbuffer at slot 0. Engine c0..c3 = WVP register space, c4..c7 = World, etc. (matches Plan 11-08's VertexSlot0CB layout). `setVertexShaderUserConstants(index, c0, c1, c2, c3)` writes to slot 0 at byte offset `index * 16`.

But Plan 11-08's "full-fill mandate" (CODEX Q6: Map(WRITE_DISCARD) yields UNDEFINED bytes) requires every updateVS(0, ...) to upload the FULL 1152-byte VertexSlot0CB. So we'd need a CPU-side shadow of the full slot 0 that's mutated by user-constants pushes + uploaded as one piece per draw.

In this case **Plan 11-08's matrix-setter wiring stays as dead code** (engine doesn't use the setWorld* setters), but the SLOT 0 layout was correct — just populated by the wrong path.

### Fix C: Something else?

Maybe HlslRewrite Rule D splits constants across multiple cbuffer slots (b0 for some, b1 for others), and the right fix involves understanding which slot maps to which register range.

## Questions for CODEX

> **Q1.** Which cbuffer slot does the engine HLSL (post-Plan 11-07 HlslRewrite Rule D) actually read its c0..cN constants from? Is it always slot 0 (single cbuffer), or split across slots?
>
> **Q2.** Given the engine USES `setVertexShaderUserConstants` and NEVER uses the matrix setters: Fix A (resurrect Plan 11-05's slot 1 wiring) or Fix B (merge into Plan 11-08's slot 0) or Fix C?
>
> **Q3.** If Fix A is right: is Plan 11-08's entire matrix-shadow + composeAndUploadSlot0 + Direct3d11_VertexSlot0CB machinery dead code that should be REVERTED, or kept dormant for future use?
>
> **Q4.** If Fix B is right: the full-fill mandate (CODEX Q6 from your Round 1) requires uploading all 1152 bytes per change. The engine may call setVertexShaderUserConstants dozens of times per draw; that's 60+ × 1152-byte uploads per frame. Acceptable, or do we need a dirty-flag-driven upload-once-per-draw pattern?
>
> **Q5.** Per `Direct3d11.cpp:380` the current impl gates `if (index >= 0 && index < 8)` — only 8 user-constants slots. The engine likely pushes more (WVP=4 + World=4 + lighting/material/etc.). What's the right capacity?
>
> **Q6.** The probe also revealed Plan 11-08's matrix setters are wired into `ms_glApi` but never called. Should those Gl_api bindings be removed too (or left for forward-compat with future engine refactors)?
>
> **Q7.** Plan 11-09 Iter-2 is in REVIEW state across multiple sub-iterations (Iter-2.1 through Iter-2.6 all diagnostic probes that haven't committed). Should we:
>   - (a) Commit all diagnostic-probe sub-iterations + the real fix as a single Iter-2.x bundle when working
>   - (b) Commit each sub-iteration separately for trail-of-evidence preservation
>   - (c) Commit only the working final fix and treat diagnostic iterations as throwaway transcript

## What I'm doing while you consult

Holding all code edits. The diagnostic-probe infrastructure (iter2-probe.txt + ID3D11Query) is in place. The real fix needs your input on Fix A/B/C before I write a line of it.

---

## Files cited

| File | Lines | Relevance |
|---|---|---|
| `Direct3d11.cpp` | 365-390 | `setVertexShaderUserConstants_impl` — broken (function-local-static shadow) |
| `Direct3d11.cpp` | 392-413 | `setPixelShaderUserConstants_impl` — same bug |
| `Direct3d11.cpp` | 664 | `ms_glApi.setVertexShaderUserConstants = setVertexShaderUserConstants_impl` |
| `Direct3d11.cpp` | 654-657 | `ms_glApi.setWorld*/Projection*/ObjectToWorld* = StateCache::set*` (never called by engine) |
| `Direct3d11_StateCache.cpp` | 224, 237 | `composeAndUploadSlot0` — fires once (race guard) with identity |
| `Direct3d11_ConstantBuffer.cpp` | 111 | the only `updateVS(1, zero, ...)` call (primeDefaults zero-fill) |
| `Direct3d11_HlslRewrite.cpp` | (not yet read) | Rule D cbuffer-wrap — TBD which slot it targets |

---
