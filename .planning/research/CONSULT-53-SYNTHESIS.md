# CONSULT-53 — Space HUD "cyan square" — SYNTHESIS (crew + ground-truth adjudication)

**Date:** 2026-06-28. Crew: Codex, Cursor, Sonnet, Opus (all reported). Adjudicated against asset
extraction + RenderDoc pixel ground truth (Capture47).

## What the crew proposed, and how ground truth ruled on each

| Consultant | Mechanism | Verdict |
|---|---|---|
| Opus | D3D9 alpha-test enabled (ref≈A128) discards frame; D3D11 dropped enable | **REFUTED** — `.sht` has alphaTestEnable=FALSE |
| Cursor | D3D9 asset PS (`ui.psh`) ≠ D3D11 generated PS | **REFUTED** — `ui.psh` = `ps.1.1: mul r0,t0,v0` = tex×vtx, *identical* math to generated PS 2136 |
| Sonnet | D3D11 texture decode yields different alpha | **ELIMINATED** — same DDS, same parser, same alpha 0.5725 both renderers |
| Codex | (identification, no fix) | Confirmed shader/widget identity (below) |

Cursor's **direction** (it's the UI-canvas path, systemic) is right; its **mechanism** (asset PS) is not.

## Established facts (ground truth)

- Buggy element = **`ui_space_gauge_square`** (frame/gauge fills) via **`shader/uicanvas_filtered.sht`**
  → `texture/ui_space_gauge_square.dds`. Same shader batches the chat-window chrome and other
  UI canvas fills → **systemic** (matches user report: chat chrome also cyan).
- The bright cyan reticle **ring** is a SEPARATE additive shader `ui_shader_add.sht`
  (`effect/ui_add.eft`, blend SourceColor/One) — legitimately bright, NOT the bug.
- `uicanvas_filtered.sht`: alphaBlendEnable=true, **SrcAlpha/InvSrcAlpha**, alphaTestEnable=FALSE.
  Programmable PS `pixel_program/ui.psh` = `ps.1.1 { tex t0; mul r0, t0, v0 }`.
- D3D11 draw 8080: PS 2136 = `o0 = tex.rgba × vtxColor.rgba`, alpha-test OFF (cbuffer enable=0,
  matches asset). Frame texel = gray (a=0.5725); vtxColor = teal (a=0.40); **o0 = teal @ a=0.229**.
- Backbuffer background = **(0,0,0,0) black** (correct). The "white background" seen in exported PNGs
  was a RenderDoc **export artifact** (transparent area on a white page), NOT real.

## The remaining anomaly (the actual lead)

Over a **black** background, a correct straight-alpha "over" blend of a 0.229-alpha teal fill should
composite to **(0.044, 0.080, 0.086)** — faint, ≈invisible (this is what gl05 shows). But the measured
composited UI fill (chat chrome pixel 250,760, final frame) is **(0.220, 0.329, 0.361)** — essentially
the **full** teal color, NOT scaled down by alpha. That is the signature of the source RGB being added
at ~full weight, i.e. an **effective premultiplied / SrcBlend≈ONE composite** rather than straight
SrcAlpha — which would be **systemic** across all `uicanvas_filtered` draws and explains both the cyan
square and the cyan chat chrome.

CAVEAT (not yet airtight): the chat pixel is multi-layered (stacked translucent panels), which partly
inflates intensity; and per source `Direct3d11_StaticShaderData.cpp:1664-1682` *does* call
`setAlphaBlendFactors(translateBlend(SourceAlpha)=SRC_ALPHA, INV_SRC_ALPHA, ADD)` for this pass — so
on paper the color blend should already be straight-alpha. The discrepancy between "source says
SRC_ALPHA" and "composite looks like ONE" is the crux to resolve.

## Decisive next step

Directly confirm the **OM blend state actually applied** at a `uicanvas_filtered` draw:
1. Cleanest: instrument the D3D11 UI draw (or read the bound BlendState desc) and log SrcBlend/DestBlend
   for draw 8080 — is color SrcBlend `SRC_ALPHA` or `ONE`? Suspect a **blend-factor cache-skip**: a
   prior additive draw (`ui_shader_add`, Src=SourceColor) may leave stale factors that the UI frame
   draw's `setAlphaBlendFactors` cache-compares as "unchanged" and fails to reset.
2. Or measure on an ISOLATED single-layer frame pixel (not ring-overlapped, not multi-panel) the
   value just-before vs just-after its draw over black → solve the blend factors empirically.
3. True A/B (user can do; RenderDoc can't capture D3D9): screenshot the SAME HUD element under gl05 vs
   gl11 to confirm gl05 is genuinely invisible vs merely fainter.

If confirmed, the fix is in the D3D11 UI blend application (ensure straight-alpha SRC_ALPHA reaches
these passes / invalidate the blend-factor cache after additive UI draws), NOT in the shader, texture,
or alpha-test.
