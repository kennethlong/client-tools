# Phase 18: Load-Screen Half-Texel Seam - Pattern Map

**Mapped:** 2026-05-30
**Files analyzed:** 6 candidate fix/reference sites (no NEW files — all edits land in EXISTING D3D11-plugin files)
**Analogs found:** N/A in the usual sense — this is a convention-correction phase against the *existing* D3D9 reference, not a new-subsystem build. The D3D9 path IS the analog (the byte-for-byte target to match).

> **Reading note for the planner:** This phase creates no new files and has no RESEARCH.md. The "pattern to copy from" is the D3D9 sampling convention; the "files to modify" are the D3D11 transformed-vertex → NDC conversion sites. The exact central site among the candidates is **consult-gated (D-01)** — this map ranks the candidates and hands the consult its worked code, it does NOT pre-lock the site.

---

## File Classification

| File | Role | Data Flow | Relationship to bug | Match / Notes |
|------|------|-----------|---------------------|---------------|
| `…/Direct3d11/src/win32/Direct3d11_StateCache.cpp` (c9 `viewportData` @ `setViewport`, **1593-1602**) | renderer-state / config | screen-space→NDC convention | **PRIMARY CANDIDATE FIX SITE** — the central screen-space→clip-space constant that all XYZRHW 2D draws read | Exact convention site; matches D3D9 byte-for-byte today |
| `…/Direct3d11/src/win32/Direct3d11_Device.cpp` (`beginScene` viewport doc, **662-693**) | renderer-state / config | screen-space→NDC convention | Documents the `transform2d()` math + guarantees c9 is written every frame; alternate central site | Same convention as StateCache; authoritative comment block |
| `…/Direct3d11/src/win32/Direct3d11_HlslRewrite.cpp` (Rules A-E textual rewrite) | utility (HLSL transform) | transform / text-rewrite | Alternate central site — could inject a half-pixel bias into the rewritten VS body (consult fix-path A) | Currently does NOT touch position math |
| `…/Direct3d11/src/win32/Direct3d11_VertexBufferDescriptorMap.cpp` (`buildInputElementDescForStream`, **217-232**) | renderer-data / layout | layout descriptor | XYZRHW (`isTransformed`) input-layout emit; NOT a math site (consult fix-path B would convert here, CPU-side) | Layout only; no offset math today |
| `…/Direct3d9/src/win32/Direct3d9.cpp` (`getOneToOneUVMapping` **3676-3682**; `setViewport`/VSPS `viewportData` **3238-3244**) | reference convention | screen-space→NDC + UV | **REFERENCE / D3D9 TARGET** (read-only, must stay byte-for-byte unregressed per D-04) | The convention being matched |
| `…/clientUserInterface/src/shared/core/CuiManager.cpp` (`ms_pixelOffset = -0.5f` **306**; accessor `getPixelOffset()` `CuiManager.h:177-179`) | UI engine constant | vertex-emit offset | **ROOT SOURCE of the baked `-0.5`** — applied to every UI quad in `CuiLayerRenderer.cpp` (189/241/286/333/382/434/489) and `CuiLayer_EngineCanvas.cpp` (275/426); NOT an edit target | The concrete origin of the `(-0.5,-0.5)` fingerprint |
| `…/swgClientUserInterface/src/shared/page/SwgCuiSplash.cpp` | UI mediator | request-response (page produce) | On-screen CONSUMER (produces the fullscreen quad); **NOT the bug site** | Confirmed: no half-pixel math; pure `Graphics::`/UI calls |
| `…/clientGraphics/src/win32/Graphics.cpp` (`getOneToOneUVMapping` dispatch **1675-1679**) | dispatch / facade | request-response | Dead dispatch (zero real callers, see "Dead Stub" below) | Verified dead |
| `…/Direct3d11/src/win32/Direct3d11.cpp` (`STUB(getOneToOneUVMapping)` **1056**) | install / API-table | n/a | **DEAD/FATAL STUB — do NOT implement (D-01)** | Verified fatal-stub, zero callers |

---

## Pattern Assignments

### REFERENCE — D3D9 convention to match (READ-ONLY; D-04 byte-for-byte unregressed)

**File:** `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp`

**(a) D3D9 VSPS screen-space→clip-space `viewportData`** (lines 3238-3244) — this is the formula the D3D11 plugin already transcribes:
```cpp
#ifdef VSPS
	// let the vertex shader know this information for 2d stuff
	float xOffset = ((x * 2.0f) / static_cast<float>(viewportWidth));
	float yOffset = ((y * 2.0f) / static_cast<float>(viewportHeight));
	float const viewportData[4] = { 2.0f / static_cast<float>(viewportWidth), -2.0f / static_cast<float>(viewportHeight), -1.0f - xOffset,  1.0f + yOffset };
	Direct3d9_StateCache::setVertexShaderConstants(VSCR_viewportData, viewportData, 1);
#endif
```
**Note for consult/planner:** this `viewportData` has NO `+0.5` half-pixel term in it. The half-pixel offset under D3D9 came from (i) the D3D9 rasterizer's pre-D3D10 `-0.5` texel rule AND (ii) the offset baked into the XYZRHW vertex *positions* themselves — specifically `CuiManager::ms_pixelOffset = -0.5f` (`CuiManager.cpp:306`, applied via `getPixelOffset()` to every UI quad), which produces the Phase 11 capture fingerprint `(-0.5,-0.5)…(1023.5,767.5)`. The seam is the mismatch between those baked positions and D3D11's center-sampling rasterizer — NOT a defect in this formula.

**(b) D3D9 `getOneToOneUVMapping` `+0.5f` half-texel offset** (lines 3676-3682) — reference for the texel-center convention; **this function is itself uncalled** (see Dead Stub section):
```cpp
void Direct3d9Namespace::getOneToOneUVMapping(int textureWidth, int textureHeight, float &u0, float &v0, float &u1, float &v1)
{
	u0 = (0.0f + 0.5f) / static_cast<float>(textureWidth);
	v0 = (0.0f + 0.5f) / static_cast<float>(textureHeight);
	u1 = (static_cast<float>(textureWidth - 1) + 0.5f) / static_cast<float>(textureWidth);
	v1 = (static_cast<float>(textureHeight - 1) + 0.5f) / static_cast<float>(textureHeight);
}
```

---

### PRIMARY CANDIDATE — `Direct3d11_StateCache.cpp` c9 `viewportData` (renderer-state, screen-space→NDC)

**File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp`
**read_first:** lines **1571-1606** (`setViewport`)

**Current code** (lines 1585-1602) — a verbatim transcription of the D3D9 formula above:
```cpp
	// Plan 11-09 Iter-2.7b (CODEX Round 4): write VSCR_viewportData at c9
	// of the slot 0 cbuffer. ... Formula verbatim from Direct3d9.cpp:3240-3243 (VSPS path).
	float const xOffset = (static_cast<float>(x) * 2.0f) / static_cast<float>(width);
	float const yOffset = (static_cast<float>(y) * 2.0f) / static_cast<float>(height);
	float const viewportData[4] = {
		 2.0f / static_cast<float>(width),
		-2.0f / static_cast<float>(height),
		-1.0f - xOffset,
		 1.0f + yOffset
	};
	constexpr int kVSCR_viewportData = 9;
	setVSConstants(kVSCR_viewportData, viewportData, 1);
```

**Why this is the primary candidate:** The 2D VS (`2d_texture.vsh`, `ui_radar.vsh`, etc.) computes (per the Device.cpp doc and the Phase 11 disassembly):
```
ndc.x = pixel.x * viewportData.x + viewportData.z   // = pixel.x * (2/w) + (-1 - xOffset)
ndc.y = pixel.y * viewportData.y + viewportData.w   // = pixel.y * (-2/h) + (1 + yOffset)
```
A central half-pixel correction can be folded into the `.z`/`.w` (offset) terms here — one edit, all 2D draws — by biasing the clip-space origin by half a pixel. **The sign is consult-gated (D-01); do NOT pre-lock it.** The root of the baked offset is the live named constant `CuiManager::ms_pixelOffset = -0.5f` (`CuiManager.cpp:306`, accessor `getPixelOffset()` `CuiManager.h:177-179`, applied to every UI quad vertex in `CuiLayerRenderer.cpp` and `CuiLayer_EngineCanvas.cpp`), which is what bakes `-0.5` into the `(-0.5,-0.5)…(1023.5,767.5)` fingerprint.

**Leading hypothesis (to be CONFIRMED or REFUTED by the consult, NOT the locked answer):** with `ndc.x = pixel.x*(2/w) + z`, a baked `pixel.x = -0.5` maps to `-1 - 1/w`; to behave like `pixel.x = 0` the correction is most likely `z += 1.0f/width`. Symmetrically, since `ndc.y = pixel.y*(-2/h) + w`, a baked `pixel.y = -0.5` maps to `1 + 1/h`, so the correction is most likely `w -= 1.0f/height`. This satisfies D-02 (central, not per-draw) directly. The consult MUST confirm BOTH the sign AND the magnitude against the baked `(-0.5,-0.5)` fingerprint and the D3D11 center-sampling rule — the leading hypothesis above is a starting candidate the consult validates, not a pre-locked result.

---

### ALTERNATE CANDIDATE / AUTHORITATIVE DOC — `Direct3d11_Device.cpp` `beginScene` viewport

**File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp`
**read_first:** lines **662-693**

This block (1) documents the exact `transform2d()` math the consult needs and (2) guarantees `setViewport`→c9 runs every frame before the first draw (so any StateCache fix is live from frame 0, including the splash). The actual `setViewport` call is line 693:
```cpp
	//     ndc.x = pixel.x * viewportData.x + viewportData.z
	//     ndc.y = pixel.y * viewportData.y + viewportData.w
	...
	Direct3d11_StateCache::setViewport(0, 0, ms_width, ms_height, 0.0f, 1.0f);
```
**Planner note:** if the fix lands in `setViewport` (StateCache), this call site needs no edit — it inherits the correction. List as `read_first` context, not necessarily a modify target.

---

### ALTERNATE CANDIDATE — `Direct3d11_HlslRewrite.cpp` (utility, text-transform; consult fix-path A "shader-body")

**File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_HlslRewrite.cpp`
**read_first:** lines **1-60** (Rules A-E design preamble)

Today this is a pure textual SM2→SM4 compatibility rewrite (reserved-word suffixing, register-binding stripping, `#pragma def` stripping). It does **not** touch position/half-pixel math. Consult fix-path A would inject a half-pixel term into the rewritten VS body here. **Higher blast radius / fragile** (textual injection into every VS), and would not match D3D9's "convention lives in the constant" shape — list as the consult's path-A option, not the recommended site.

---

### NOT-A-MATH-SITE — `Direct3d11_VertexBufferDescriptorMap.cpp` XYZRHW layout (consult fix-path B "CPU NDC conversion")

**File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferDescriptorMap.cpp`
**read_first:** lines **217-232** (`buildInputElementDescForStream`, transformed branch)

```cpp
	if (format.hasPosition())
	{
		if (format.isTransformed())
		{
			// Transformed vertices use the SV_POSITION-equivalent xyzrhw
			// layout; pack as float4 covering xyz + rhw together. ...
			add("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(float) * 4);
		}
		else
		{
			add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, sizeof(float) * 3);
		}
	}
```
This is the `isTransformed()` (XYZRHW) detection + layout emit — it tells the consult **where the transformed path is identified**, but it carries no offset math. Fix-path B (CPU-side NDC pre-conversion of the vertex stream) is the *most invasive* option and the least aligned with "central, not scattered"; include only as the consult's path-B option.

---

## Shared Patterns

### gl11-only / D3D9 byte-for-byte invariant (D-04)
**Source:** the D3D9 reference excerpts above (`Direct3d9.cpp:3238-3244`, `:3676-3682`).
**Apply to:** every plan in this phase. The D3D9 file is **READ-ONLY** — it is the byte-for-byte unregressed reference, never an edit target. Any edit lands only under `…/Direct3d11/…`.

### Central convention, not per-draw fudge (D-02)
**Source pattern:** D3D9 keeps the convention in a single constant (`viewportData`) consumed by all 2D shaders.
**Apply to:** mirror that shape — correct the half-pixel **once** (preferred: the c9 `viewportData` `.z`/`.w` terms in `Direct3d11_StateCache.cpp:setViewport`). NO `+0.5` sprinkled at call sites.

### Full-2D no-regression validation (D-03)
**Why:** the XYZRHW path drives **all** D3D11 2D (HUD, fonts, radial menu, in-game menus, char-select 2D) — confirmed by the `isTransformed`/`xform=1` draw-logging in `Direct3d11_StateCache.cpp` (lines 1252-1308, `s_iter26CountUI` "UI" path). A central c9 edit touches all of them.
**Apply to:** the validation plan — gate on (a) seam gone on world load screen AND (b) no shift/blur on HUD/fonts/radial/char-select 2D.

### CODEX + Cursor consult gates the site (D-01 / D-01a)
**Source pattern:** prior `CONSULT-17-07-*` workflow.
**Apply to:** the first plan — assemble the brief (Phase 11 XYZRHW diagnosis + the `CuiManager::ms_pixelOffset = -0.5f` root-cause chain + the dead-`getOneToOneUVMapping` finding below + the candidate sites above + the "central" constraint), save outputs under `.planning/research/CONSULT-18-*{,-codex,-cursor}.out`, THEN lock the site AND the bias sign/magnitude.

---

## Dead Stub — DO NOT IMPLEMENT (CONTEXT D-01 VERIFIED ✅)

`getOneToOneUVMapping` was searched across the whole tree. **Confirmed zero real callers.** The only references are the plumbing of the function itself:

| Reference | File:line | Kind |
|-----------|-----------|------|
| Declaration | `src/engine/client/library/clientGraphics/src/win32/Graphics.h:218` | header decl |
| Dispatch wrapper | `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp:1675-1679` | facade → `ms_api->getOneToOneUVMapping` |
| API fn-ptr slot | `src/engine/client/library/clientGraphics/src/win32/Gl_dll.def:186` | function-pointer table entry |
| D3D9 impl | `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp:3676-3682` (+ install `:1214`, decl `:356`) | reference impl (also uncalled) |
| **D3D11 STUB** | `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:1056` | `STUB(getOneToOneUVMapping)` |

`STUB` expands (line 935) to:
```cpp
#define STUB(slot) ms_glApi.slot = reinterpret_cast<decltype(ms_glApi.slot)>(scaffold_fatal_stub)
```
where `scaffold_fatal_stub` is `[[noreturn]]` (line 114). So if the load screen ever routed through `Graphics::getOneToOneUVMapping` under D3D11 it would **FATAL**, not produce a seam — proving the splash does **not** route through it. **Implementing this stub fixes nothing visible (D-01).** Leave it as a fatal stub; defer any real impl to Phase 20 minimap against real call sites (CONTEXT Deferred).

---

## On-Screen Consumer (NOT the bug site)

`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiSplash.cpp` is a `CuiMediator` that builds the splash page from UI data (`getCodeDataObject`, `UIPage`/`UIText`) and includes `clientGraphics/Graphics.h`. It contains **no half-pixel / vertex math** — it produces the fullscreen quad that the renderer then transforms. Useful only to reach the repro deterministically; **not** an edit target.

---

## No Analog Found

Not applicable — this phase modifies existing renderer-plugin code to match an existing reference (the D3D9 path). No file lacks a reference convention.

---

## Drift / Verification Flags for the Planner

| Claim source | Claim | Verified against live code? | Note |
|--------------|-------|------------------------------|------|
| CONTEXT D-01 | `getOneToOneUVMapping` has zero callers | ✅ VERIFIED | Only self-plumbing refs (table above); no game/UI caller |
| CONTEXT D-01 | D3D11 entry is `scaffold_fatal_stub` | ✅ VERIFIED | `STUB` macro (Direct3d11.cpp:935) → `[[noreturn]] scaffold_fatal_stub` (`:114`) |
| CONTEXT canonical_ref | `Direct3d11.cpp:1056` = `STUB(getOneToOneUVMapping)` | ✅ EXACT | Line 1056 confirmed |
| CONTEXT canonical_ref | `Direct3d9.cpp:3676-3682` = `getOneToOneUVMapping` +0.5 | ✅ EXACT | Lines confirmed |
| CONTEXT canonical_ref | `Graphics.cpp:1675-1679` = dispatch | ✅ EXACT | Lines confirmed |
| Review (orchestrator-verified) | baked `-0.5` source = `CuiManager::ms_pixelOffset = -0.5f` | ✅ VERIFIED | `CuiManager.cpp:306`; decl `CuiManager.h:150`; accessor `getPixelOffset()` `CuiManager.h:177-179`; applied via `getPixelOffset()` in `CuiLayerRenderer.cpp` (189/241/286/333/382/434/489) and `CuiLayer_EngineCanvas.cpp` (275/426) — exact apply site to be pinned by the executor |
| CONTEXT canonical_ref | central fix lives in HlslRewrite / VertexShaderData / StateCache / Device / VertexBufferDescriptorMap | ✅ CONFIRMED + REFINED | The XYZRHW→NDC **math** is the c9 `viewportData` constant in `StateCache.cpp:1593-1602` (consumed by the engine-authored 2D VS via `transform2d()`); `VertexShaderData`/`HlslRewrite`/`DescriptorMap` are supporting paths, not where the offset currently lives |
| **STALE — research artifacts** | `getOneToOneUVMapping` stub at `Direct3d11.cpp:995` (SUMMARY.md, CONSULT-slot0-*, ARCHITECTURE.md) | ⚠️ DRIFTED | The line number **drifted from 995 → 1056**. CONTEXT.md already uses the correct `:1056`; if planner pulls from `.planning/research/SUMMARY.md` or `CONSULT-slot0-*`, those still say `:995` — use **1056**. 18-01 has a task to reconcile the stale SUMMARY.md phrasing. |
| **STALE — research artifacts** | "viewportData (c9) in Direct3d11_StateCache.cpp:1504-1511" (SUMMARY.md:128) | ⚠️ DRIFTED | The c9 write is now at `StateCache.cpp:1593-1602`, not `1504-1511`. Use the verified range. |
| CONTEXT specifics | baked quad fingerprint `(-0.5,-0.5)…(1023.5,767.5)` | ⚠️ NOT RE-CAPTURABLE FROM SOURCE | Comes from the live Phase 11 consult capture; the underlying `-0.5` IS source-verifiable (`ms_pixelOffset`, above). Carry the fingerprint into the consult brief as given. |
| CONTEXT D-04 | the c9 formula is "byte-for-byte from D3D9" | ✅ VERIFIED | `StateCache.cpp:1593-1602` matches `Direct3d9.cpp:3240-3242` exactly; confirms the seam is NOT in this formula — it is the baked-position vs center-rasterizer mismatch |

---

## Metadata

**Analog search scope:** `src/engine/client/application/Direct3d11/src/win32/`, `…/Direct3d9/src/win32/`, `…/clientGraphics/src/win32/`, `…/clientUserInterface/src/shared/core/`, `…/swgClientUserInterface/src/shared/page/`; whole-tree grep for `getOneToOneUVMapping` and `ms_pixelOffset`/`getPixelOffset`.
**Files scanned:** 9 source files (read) + whole-tree grep for the dead symbol + the pixel-offset chain.
**Pattern extraction date:** 2026-05-30 (review-hardened)
