# Pitfalls Research

**Domain:** D3D9 -> D3D11 asset pixel-shader + visual-parity porting in the SWG/whitengold legacy game-engine client (`gl11_d.dll` plugin; `gl05_d.dll` D3D9 is the known-good reference)
**Researched:** 2026-05-27
**Confidence:** HIGH (most pitfalls are *already-confirmed in this engine* — Phase 11 iteration log + memory record + direct code reads; D3D9-vs-D3D11 semantic facts cross-checked against the live plugin source and D3D9 sibling)

> **Scope note for the roadmapper:** This is a parity port, not a clean-room shader rewrite. The single most important meta-pitfall is **"correct by D3D11 spec" != "matches D3D9 output."** The engine and its assets were authored against D3D9 behaviour (including bugs — see the deliberate `Compare[]` swap). Every D3D11 decision must be validated against the D3D9 sibling's *actual* behaviour, not against what the API "should" do. Phase ordering should follow the COMPARISON.md beachhead: character-select textures + eyes first, then world.

---

## Critical Pitfalls

### Pitfall 1: Gamma DOUBLE-correction — the "blown-out flat white" prime suspect

**What goes wrong:**
D3D11 interiors render as washed-out flat white (COMPARISON.md bucket 2), while D3D9 shows correct ambient/diffuse interior lighting and atmospheric haze. The naive D3D11 fix — "use an `_SRGB` backbuffer/SRV so colours are correct" — makes it *worse*, because it linearises on sample AND on output where the content never expected either.

**Why it happens:**
D3D9 in this engine applies gamma **only at scanout**, via `IDirect3DDevice9::SetGammaRamp` (a hardware LUT on the way to the monitor — `Direct3d9.cpp:2198 setBrightnessContrastGamma`), and it explicitly sets **`D3DSAMP_SRGBTEXTURE = 0`** (`Direct3d9_StateCache.cpp:193,424`) so textures are sampled **raw** (no de-gamma on read) and the framebuffer is a plain `B8G8R8A8_UNORM` non-sRGB surface. All shader math runs in the texture's native (gamma) space; the ramp is a final cosmetic curve. D3D11/DXGI has **no `SetGammaRamp` equivalent** for the swapchain, so a porter is tempted to instead: (a) make the backbuffer `_SRGB`, and/or (b) sample textures through `_SRGB` SRVs, and/or (c) add an output LUT — and if more than one of those is applied, colours are corrected twice -> blown out. The current plugin has gamma as a **no-op stub** (`Direct3d11.cpp:253 setBrightnessContrastGamma_impl`) and uses **non-sRGB** formats everywhere (backbuffer `DXGI_FORMAT_B8G8R8A8_UNORM` at `Direct3d11_Device.cpp:313`; texture table is all `_UNORM`, zero `_SRGB`, at `Direct3d11_TextureData.cpp:56-77`), so today there is *no* gamma curve at all — interiors look flat-white partly because the LUT that D3D9 applies is simply missing, and partly because PS lighting is stubbed (Pitfall 5/9).

**How to avoid:**
Replicate D3D9's model exactly: **sample raw (non-sRGB SRVs), do all math in gamma space, apply the brightness/contrast/gamma curve ONCE as a final full-screen post-process** (a fullscreen pass over the rendered backbuffer using the same `pow(0.5 + contrast*((f*brightness)-0.5), 1/gamma)` formula from `Direct3d9.cpp:2207`, or a 256-entry 1D LUT built from it). Do **not** flip the backbuffer or texture SRVs to `_SRGB` as a shortcut — that changes sampling and blending math the assets never expected. Keep `D3DSAMP_SRGBTEXTURE`-off parity: the D3D11 samplers are already raw, leave them raw. Separate this from the PS-lighting work: the LUT may be independent of the asset-PS bug (COMPARISON.md explicitly says "track separately in case the LUT is independent").

**Warning signs:**
- Whole scene uniformly brighter/whiter than D3D9, *including* UI and load screens (a global curve problem, not a per-material one).
- Toggling an sRGB SRV makes mid-tones shift in the wrong direction (double-correction).
- Dark areas crush to black while highlights blow out (an sRGB-applied-twice S-curve signature).
- `setBrightnessContrastGamma_impl` still a no-op while the scene is too bright -> the missing LUT, not double-correction; if you've *added* a LUT and it's still too bright, suspect a second correction sneaking in via format.

**Phase to address:**
Dedicated **Gamma/LUT + lighting** phase, sequenced *after* the asset-PS pipeline lands on character-select (so you're correcting a correctly-shaded image, not a magenta one). Verify on the character-select screen + a known interior A/B against D3D9.

---

### Pitfall 2: Half-texel offset — the load-screen centerline seam (D3D9 -0.5 rule, removed in D3D10+)

**What goes wrong:**
A faint vertical seam appears dead-center (x~512) on D3D11 load screens (COMPARISON.md bucket 5). Confirmed **image-independent and renderer-specific** — many D3D11 splashes show it, ~20 D3D9 load-ins never did — so it's a path-level sampling/blit bug, not a bad texture.

**Why it happens:**
D3D9's rasterizer placed pixel centers at integer coordinates, so fullscreen blits and screen-space UI needed the canonical **-0.5 texel offset** on output vertex positions to align texels to pixels. D3D10+ moved pixel centers to (x+0.5, y+0.5) and **removed the -0.5 rule**. The SWG load screen is drawn as **two side-by-side halves** (the splash exceeds max single-texture width), so each half is a screen-space quad; if the engine's screen-space vertex positions still carry the D3D9 half-pixel bias (or the D3D11 path fails to account for the convention change), the shared inner edge of the two halves samples a half-texel off, leaving a seam exactly at center. With clamp addressing, the seam is a duplicated/missing column; with wrap, it can be a colour bleed.

**How to avoid:**
Decide the convention **once**, centrally: either (a) bake the half-pixel adjustment into the screen-space/UI viewport-to-clip transform (the `setViewport` c9 `viewportData` already does the screen-space->clip mapping at `Direct3d11_StateCache.cpp:1504-1511` — that's the right single chokepoint), or (b) ensure the two-half blit uses **clamp** addressing with UVs computed so the inner edges share exactly the texel boundary. Do NOT scatter +0.5 fudge factors into individual draw paths. Use the load screen as the deterministic isolation harness (no 3D, no skinning, no lighting) before touching in-world UV math.

**Warning signs:**
- One-pixel-wide seam/duplication at a texture-tile boundary, stable across different images.
- UI text/icons look subtly soft or shifted half a pixel vs D3D9 (same root cause leaking into the HUD).
- Seam moves or vanishes when you nudge the viewport `viewportData` offsets by a half-texel — confirms the convention is the lever.

**Phase to address:**
**Load-screen / 2D-blit phase** (small, isolated, deterministic — good standalone win and sampling-path canary). Owns the screen-space->clip half-pixel convention for the whole 2D path. Verify: zone repeatedly, confirm no seam on multiple splash images, and confirm HUD text crispness matches D3D9.

---

### Pitfall 3: Matrix transpose at cbuffer upload — the dual-convention trap (ALREADY HIT, will recur)

**What goes wrong:**
Any matrix uploaded to a D3D11 cbuffer without `XMMatrixTranspose` produces catastrophic clip-space coordinates — the "radial-fan stretched-sliver" symptom that uniformly broke all in-world geometry at Iter-38A (resolved Iter-38B).

**Why it happens:**
The SOE engine builds matrices in **column-vector convention** (translation in the last *column*, e.g. `Transform.getMatrix()[*][3]`), but HLSL bytecode compiled with `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` does **row-vector left-multiply** (`pos_row * M`). These are duals: `pos_row * stored` only equals `M * pos_col` when `stored = transpose(M)`. This is a downstream-of-packing issue — and it is a *known consult trap*: the row/col-major packing hypothesis "checks out" (the `#pragma`/`row_major` disassembly looks right) while the actual content-vs-multiply dual is the real bug. CODEX+Cursor both missed it twice before empirical clip.w instrumentation forced the conclusion.

**How to avoid:**
**Every** matrix added to any D3D11 cbuffer (WVP, World, and any *new* one — bone palettes, shadow/reflection projections, texture matrices) MUST be `XMMatrixTranspose`'d before `XMStoreFloat4x4`. Canonical pattern: `composeSlot0Shadow` in `Direct3d11_StateCache.cpp`. Do not be reassured by `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` being set or `row_major` appearing in disassembly — those describe *packing*, not the multiply convention.

**Warning signs:**
- Geometry collapses to a fan from the origin, or stretches into slivers toward a point.
- A new matrix-driven feature (skeletal bones, projected shadows, scrolling-texture matrix) renders garbage while the rest of the scene is fine — you added a cbuffer matrix and forgot the transpose.
- clip.w goes hugely negative for vertices that should be in front of the camera.

**Phase to address:**
Cross-cutting; flag in **any** phase that adds a cbuffer matrix. Particularly the **skeletal/eyes** phase (bone matrices) and any **reflection/shadow** work. Verify by capturing a known WVP and checking clip.w sign both ways (`M*pos` vs `pos*M`).

---

### Pitfall 4: PS-input ⊆ VS-output is a HARD linkage contract, by semantic AND register position (ALREADY HIT)

**What goes wrong:**
Draws silently vanish (all black) or get rejected by the runtime when the pixel shader declares an input the vertex shader doesn't output at the *exact* semantic index AND hardware register. Iter-1 of the dynamic-PS work produced **483,451 `id=342` errors/session**; the register-position fix still left **65,034 `id=343`/session** until the PS was generated register-for-register from the VS reflection.

**Why it happens:**
D3D9 was permissive about shader linkage; D3D11 enforces `D3D11_FATAL_INPUT_SUBSET` — the PS input signature must be a subset of the VS output signature, matched by semantic name+index **and** by the hardware register the compiler assigned. The engine ships *pre-compiled D3D9 PEXE bytecode* that `CreatePixelShader` rejects, so the plugin **generates** a PS per VS. A hand-authored or "one-size" fallback PS will mismatch most VSes' output layouts.

**How to avoid:**
Keep the established Bucket-2 approach: reflect each VS's output signature, sort outputs by `Register`, and generate the PS input struct **register-for-register** (the compiler assigns PS inputs in declaration order). See `Direct3d11_PixelShaderProgramData.cpp:350-513`. Cache compiled PS by VS bytecode hash; tombstone compile failures. When extending the generator (e.g. to add lighting/multi-stage sampling), preserve the reflection-driven input struct — don't hardcode `TEXCOORD0 at v0`.

**Warning signs:**
- `d3d11-debug.log` floods with `id=342` (semantic subset violation) or `id=343` (register-position mismatch).
- A specific asset class renders black/invisible while others are fine (its VS has an output layout the generator mishandles).
- `PSInvocations == 0` despite non-zero `CPrimitives` (draws submitted, nothing shaded).

**Phase to address:**
**Asset-PS pipeline** phase (the #1 blocker, owns the generator). Any later phase that touches PS generation must re-run the `id=342/343 == 0` gate. Verify via InfoQueue drain to `stage/d3d11-debug.log`.

---

### Pitfall 5: Magenta fallback masking the real PS gap — "geometry shows, so shading must be close" fallacy

**What goes wrong:**
Surfaces render flat-white/mauve where D3D9 shows full diffuse textures, with scattered **magenta patches** (COMPARISON.md bucket 1). It's tempting to treat the magenta as a few stray bad shaders; in reality the magenta is the *universal fallback PS* firing wherever the per-VS generator can't produce a correct sampling body, and the flat-white surfaces are the generator producing a body that samples but does **no material/lighting/multi-stage math**.

**Why it happens:**
The dynamic PS generator currently emits only: `t.Sample(s, uv)`, `tex*color0` modulate, vertex-color passthrough, or magenta (`Direct3d11_PixelShaderProgramData.cpp:447-509`). It binds **diffuse at slot 0** and (since Iter-44E) other stages at slots 1..7 (`Direct3d11_StaticShaderData.cpp:717-736`) — but the generated PS **does not sample slots 1..7**, ignores `material`/`textureFactor`/`textureScroll`, and does no lighting. So multi-stage materials (eyes, terrain detail blends, specular/normal/glow, the round minimap mask) look wrong even though "textures appear." VSes lacking `TEXCOORD0` (vertex-color-only UI, particles, debug lines, non-UV coordinate gen) fall to magenta.

**How to avoid:**
Treat "textured but wrong" and "magenta" as the **same** unfinished-PS-pipeline problem, not two bugs. Prioritise a generator (or a real asset-PS bridge) that consumes the engine's `ShaderImplementation` pass description: multi-stage texture combiners, `material`/`textureFactor` constants, alpha-test, and lighting. Mirror the D3D9 sibling's `Pass::apply` + `Stage::apply` constant uploads (`Direct3d9_StaticShaderData.cpp`). Use the lifetime counters already in place (`s_passesWithSampler0TextureResolved`, `ms_drawsWithSRV0Bound`, Iter-44E multi-stage log) to see which assets exercise slots > 0.

**Warning signs:**
- Eyes render gray/through-the-head; minimap stays square; specular/glow absent — all multi-stage-sampling tells.
- Magenta correlates with non-textured geometry (particles, lines, UI fills) -> generator hit the magenta branch.
- `Iter-44E multi-stage` log shows `maxSlot > 0` with `_` (present but unresolved) entries.

**Phase to address:**
**Asset-PS pipeline** phase, with **multi-stage sampling** as a tightly-coupled sub-area (and **minimap** + **eyes** as acceptance cases). Verify against D3D9 A/B on character-select first (eyes), then world.

---

### Pitfall 6: Render-state defaults silently diverge from D3D9 (per-pass blend / depth / alpha-test / color-write)

**What goes wrong:**
Subtle parity drift: content that should be opaque is translucent (or vice-versa), eyes/decals depth-fail "through the back of the head," washed-out particles. A *global* blend-default-on (Iter-32A) once masked the matrix bug AND washed world content — a classic "fixed one symptom, introduced another."

**Why it happens:**
D3D9 pushes per-pass render states via `RSB/RSM` writes (`ZENABLE`, `ZWRITEENABLE`, `ZFUNC`, `COLORWRITEENABLE`, blend factors, alpha-test). D3D11 has **immutable state objects** and **no fixed-function alpha-test**, so a porter must (a) translate each engine pass's state into BS/DSS/RS descriptors per draw, and (b) emulate alpha-test in the PS via `clip()`. If the per-pass apply site is wrong, *install-time defaults* leak onto every draw. The engine has a dead `Direct3d11_ShaderImplementationData::apply()` (no virtual base — nothing calls it); the **real** per-draw apply site is `Direct3d11_StaticShaderData::apply(passNumber)` (`:587`). Wiring state to the dead method = silent no-op using defaults.

**How to avoid:**
Route all per-pass state through `Direct3d11_StaticShaderData::apply` (blend enable `:639`, alpha-test `:660`, depth enable/write/compare `:675-677`, color-write `:684`). **Mirror the D3D9 sibling's actual mapping**, including the deliberate `Compare[]` swap (see Pitfall 7). Note the per-pass blend *factors* re-land (Iter-44C) was correctly **reverted** — it amplified the multi-stage-PS gap; do NOT re-enable blend factors until the PS pipeline samples slots 1..7 correctly. State objects are hash-cached (`getOrCreateBS/DSS/RS`), so **zero-initialise every descriptor** (`= {}`) before filling — uninitialised padding poisons the cache key and explodes the cache (Plan 11-06 Risk #3).

**Warning signs:**
- A pass renders with default depth (LESS_EQUAL, write-all) when the asset wanted Z-disabled/Always -> "eyes through head."
- Re-enabling blend factors brightens/whitens the scene (snow patches, bigger white particles — the Iter-44C regression signature).
- State-object cache count balloons (a `getOrCreateBS` miss per draw) -> a descriptor field isn't zeroed.

**Phase to address:**
Folded into **asset-PS pipeline** (state travels with the pass) and revisited in **particles/ribbon**. Verify: A/B the eye pass and a known alpha-tested foliage/decal vs D3D9; watch state-object create counts in `Direct3d11_Metrics`.

---

### Pitfall 7: "Fixing" the deliberate Compare[] swap (and other authored-against-bug behaviours)

**What goes wrong:**
Someone sees `C_GreaterOrEqual -> D3D11_COMPARISON_NOT_EQUAL` and `C_NotEqual -> D3D11_COMPARISON_GREATER_EQUAL` in `translateCompare` (`Direct3d11_StaticShaderData.cpp:169-170`), assumes it's a typo, "fixes" it to match the enum names — and breaks content that's been authored against the swap for years.

**Why it happens:**
The D3D9 sibling's `Compare[]` table (`Direct3d9_ShaderImplementationData.cpp:31-40`) has these two entries swapped relative to the enum comments. Assets (depth/alpha/stencil compare funcs) were authored against that *actual* behaviour. The enum comments lie. CODEX flagged this in the Iter-44 deep-dive; the swap is **intentional parity**, not a bug to fix.

**How to avoid:**
Treat the D3D9 sibling's runtime behaviour as ground truth over enum comments / API "correctness." Mirror any Compare-based translation (depth func, alpha-test func, stencil func) including the swap. More broadly: before "correcting" any D3D11 mapping, diff against the D3D9 sibling and ask "would this change what the asset sees?"

**Warning signs:**
- A depth/stencil/alpha-test "cleanup" commit changes how foliage/decals/transparency sort or clip vs D3D9.
- You're about to edit a translation table because the comment disagrees with the enum name.

**Phase to address:**
Cross-cutting guard in **asset-PS pipeline** and **particles**. Document the swap inline (already done) and gate any compare-table edit on a D3D9 A/B.

---

### Pitfall 8: Single-stream skeletal path — shard/stretch geometry twin of the c0000005 crash

**What goes wrong:**
Skeletal characters (NPCs, the char-select avatar) render as **white shards/spikes** in D3D11 (COMPARISON.md bucket 3). This is the *render-side twin* of the already-fixed `c0000005` use-after-free (commit `905fb5d64`): same branch, `SoftwareBlendSkeletalShaderPrimitive` with `ms_useMultiStreamVertexBuffers == false`.

**Why it happens:**
The D3D11 plugin reports `supportsStreamOffsets = false` / `maxStreamCount = 1`, so `SoftwareBlendSkeletalShaderPrimitive::install` takes the **single-stream** branch (D3D9 on real HW takes multi-stream). The crash was `collide` *reading* a freed/reused single-stream vertex buffer; the stretch is the *draw* path skinning into bad/misaligned vertex positions on the same branch. The fix for the crash was defensive guards, **not** flipping the caps — so the draw-side distortion likely persists.

**How to avoid:**
Decide deliberately between two routes: (a) make the D3D11 plugin report multi-stream support (the *not-taken* alternative from the crash fix — `Direct3d11_VertexBufferVectorData`, Plan 11-09.7), aligning skinning with D3D9's known-good multi-stream path; or (b) correct the single-stream skinning math/buffer layout so the draw path produces correct positions. Capture a skeletal draw in RenderDoc and inspect the post-skinning vertex positions vs D3D9. Note: distinguish from the **exterior static-mesh** shards (0014 wreck) which may be a *separate* (non-skeletal) distortion — re-capture fully-settled frames first (0013/0014 were mid-load, some smear is LOD streaming).

**Warning signs:**
- Skeletal meshes spike/shard while static world geometry is coherent -> single-stream skinning path.
- The shard pattern is stable per-character (not transient streaming smear).
- Static large objects *also* shard -> a broader distortion, investigate separately (OPEN per COMPARISON.md).

**Phase to address:**
**Skeletal/eyes** phase (character-select beachhead). Verify with a RenderDoc mesh-viewer capture of a skinned draw, A/B against D3D9. Re-capture settled exterior frames to scope the static-mesh question into its own investigation.

---

### Pitfall 9: Missing engine constant uploads — `material`, `textureFactor`, light data, fog, texture scroll

**What goes wrong:**
Even with a sampling PS, materials look flat/unlit, glow/specular missing, fogged scenes lack depth haze, scrolling textures (water, conveyor) sit still. The geometry and base texture are right; the *material math* is absent.

**Why it happens:**
The X4016 saga (Phase 11) was about the engine's `vertex_shader_constants.inc` declaring ~15 D3D9-era globals at explicit `register(cN)` (Material c11, LightData c16, textureFactor c44 ... b0..b7) for programmer-managed constant space. The plugin's PS generator currently reads **none** of these (`apply()` explicitly defers "material / textureFactor / textureScroll / alpha-test / stencil / fog" to Phase 12). The D3D9 sibling uploads them in `Pass::apply` / `Stage::apply`. Until D3D11 uploads the same constants into the matching cbuffer slots, lit/material content can't match.

**How to avoid:**
Port `Pass::apply` constant uploads from the D3D9 sibling into the D3D11 per-pass apply, filling the slot-0 cbuffer regions the rewritten HLSL expects (the cbuffer-wrap + reflection-driven offsets from the X4016 fix established the layout). Every uploaded matrix still needs the transpose (Pitfall 3). Light data and fog feed the lighting math that, together with the gamma LUT (Pitfall 1), fixes the flat-white interiors.

**Warning signs:**
- Surfaces textured but uniformly unlit / no specular / no rim.
- Animated/scrolling-UV materials are static.
- Distant geometry lacks fog falloff vs D3D9.

**Phase to address:**
**Asset-PS pipeline** + **gamma/lighting** phases (constants and the LUT together unblock interiors). Verify per-material against D3D9 A/B; instrument cbuffer contents in RenderDoc.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems in *this* port.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Magenta/passthrough fallback PS for any VS the generator can't handle | Geometry visible, client boots, unblocks downstream work | Masks which asset classes still lack real shading; "looks mostly fine" hides eyes/minimap/particle gaps | As a *visible diagnostic*, never as the parity endpoint — keep it but track residual magenta as open work |
| Flipping backbuffer/SRVs to `_SRGB` to "get gamma right" | One-line, colours shift toward "correct" | Double-correction (Pitfall 1); diverges from D3D9's sample-raw + LUT-at-scanout model; breaks blend math | Never — replicate the D3D9 LUT post-process instead |
| Global render-state defaults (blend-on, depth LESS_EQUAL) instead of per-pass | Lots of content renders immediately | Silently washes/over-occludes content; masks other bugs (the Iter-32A trap) | Only as a temporary bring-up step, with a TODO to wire per-pass before any parity claim |
| Re-enabling per-pass blend *factors* before multi-stage PS sampling works | Matches D3D9 blend descriptors on paper | Amplifies the unsampled-slot PS gap -> brighter/washed regression (Iter-44C, correctly reverted) | Only after the PS samples slots 1..7 and material constants upload |
| Reporting `maxStreamCount=1` and guarding the skeletal path defensively | Stops the c0000005 crash without caps surgery | Leaves single-stream skinning shard/stretch unresolved | Acceptable for crash-safety; revisit caps for visual parity |
| Hand-tuning +0.5 texel fudges per-draw to kill the seam | Seam disappears on the one screen you tested | Half-pixel bias leaks inconsistently into HUD/UI; whack-a-mole | Never — fix the screen-space->clip convention once at the viewport transform |
| Non-zeroed D3D11 state descriptors before hash-caching | Less typing | Padding bytes poison the cache key -> per-draw state-object churn (Plan 11-06 Risk #3) | Never — always `D3D11_..._DESC d = {};` |

## Integration Gotchas

Connecting the D3D11 plugin to the existing engine and shared code.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Plugin DLL calling clientGraphics members | Use the public getter chain (`getShaderEffect().get...`) — LNK2019, methods are non-inline non-DLLEXPORT | Direct field access via `friend` declarations, mirroring the D3D9 sibling's friend set (4 engine headers) |
| Wiring per-pass state | Wire to `Direct3d11_ShaderImplementationData::apply()` | That method is **dead code** (no virtual base); the live per-draw site is `Direct3d11_StaticShaderData::apply(passNumber)` |
| Editing shared engine code (clientGraphics, skeletal, shader abstraction) for the D3D11 fix | Change shared math/state and regress the working D3D9 path | Gate the renderer behind the plugin where possible; if shared code must change, **boot-test BOTH `rasterMajor=5` and `=11`** before claiming done (the project invariant) |
| Koogie's diagnostic/modernization patches | Revert/weaken a Koogie patch when its strict assert fires | Fix the **caller/data/underlying bug**; Koogie's stricter behaviour is surfacing real latent bugs by design |
| Pre-compiled D3D9 PEXE pixel bytecode | Feed it to `CreatePixelShader` | It's rejected (Plan 11-05 caveat); generate/bridge a real D3D11 PS instead |
| Async `.mgn` loads | Chase the intermittent `0x087a` crash as a renderer bug | It's **pre-existing and NOT D3D11-related** (cross-client confirmed); don't burn cycles on it |
| `CopySubresourceRegion` between RT and texture | Reuse the D3D9 `X8R8G8B8` (BGRX) format for the persistent baked RT | D3D11 strict-rejects across typeless families; the baked RT must be **BGRA8** (`B8G8R8A8_UNORM`), matching `TF_ARGB_8888` dest |

## Color / Format Parity Traps

D3D9->D3D11 component-ordering and format facts that silently corrupt colour if mishandled.

| Trap | Symptoms | Prevention | Status in plugin |
|------|----------|------------|------------------|
| Vertex color D3DCOLOR/PackedArgb ordering | Red/blue swapped on vertex-colored UI/particles | Engine `PackedArgb` is ARGB-packed == `DXGI_FORMAT_B8G8R8A8_UNORM`; HLSL `.rgba` then reads engine's (a,r,g,b) — use B8G8R8A8 for COLOR0/COLOR1 input-layout elements | Correct: `Direct3d11_VertexBufferDescriptorMap.cpp:167-173, 240-244` |
| sRGB texture sampling | Mid-tones shifted; interacts with gamma double-correction | Keep all texture SRVs non-sRGB `_UNORM` to mirror D3D9's `D3DSAMP_SRGBTEXTURE=0` | Correct: texture table is all `_UNORM` (`Direct3d11_TextureData.cpp:56-77`) |
| Backbuffer format | Wrong blend/output gamma | Non-sRGB `B8G8R8A8_UNORM` to match D3D9 PackedArgb framebuffer | Correct: `Direct3d11_Device.cpp:313` |
| `L8` luminance textures | Black where D3D9 showed gray luminance | `TF_L_8 -> R8_UNORM`, sample with `.rrr` swizzle in the generated PS | Mapped (`Direct3d11_TextureData.cpp:71`); confirm the PS swizzles |
| Premultiplied/24bpp formats (DXT2/4, RGB_888, P_8) | Asset fails to create texture, falls to fallback | These map to `DXGI_FORMAT_UNKNOWN`; engine promotes via `runtimeFormats` | Mapped as UNKNOWN by design; verify the engine's promotion path is exercised |
| BC sub-block bottom mips | `CreateTexture2D (staging)` E_INVALIDARG on small mips | Pad staging desc to 4x4 block boundary for BC formats | Handled (`isBlockCompressedFormat` pad in `lock()`) |

## Performance / Scale Traps

This is a single-client renderer; "scale" = scene complexity, draw count, and shader-permutation count.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Shader permutation explosion | Hitch on first sight of each new VS; growing compile time; cache misses | Cache compiled PS by VS bytecode hash; persist `.cso` to disk (`Direct3d11_ShaderCache`); tombstone failures; include `D3D11_REWRITE_VERSION` in the hash key so cache invalidates on generator changes | When a zone introduces many distinct VS output layouts (open-world, vs the bounded char-select set) |
| State-object cache poisoning | `getOrCreateBS/DSS/RS` miss per draw; create count climbs | Zero-init every descriptor before hashing (Plan 11-06 Risk #3) | Immediately, if any descriptor field is uninitialised |
| fxc compile-time stalls in-frame | Frame spikes when entering new areas | Compile on first-encounter + cache to disk; consider pre-warm for the char-select set | Open-world streaming of varied materials |
| Per-frame InfoQueue drain volume | Log floods (483k id=342/session at worst) | Fix the underlying linkage so the queue is quiet; keep one-shot/capped diagnostics (the code already caps several at 50/100) | When a regression reintroduces id=342/343/353 floods |

## Debugging Without Source — Tooling Strategy

The asset shaders ship as opaque pre-compiled D3D9 PEXE bytecode (no HLSL source), and explorer-launched smokes lose `OutputDebugString`/stdout. Debug accordingly.

| Need | Wrong approach | Right approach |
|------|----------------|----------------|
| See what's actually bound/drawn per frame | Guess from symptoms | **RenderDoc** frame capture — D3D11 fully supported (up to 11.4); inspect input/output textures per draw, SRV/sampler/cbuffer state, and the mesh viewer for pre/post-VS vertex positions (skeletal shards) |
| Step a shader | printf-style guessing | RenderDoc **vertex & pixel shader debugging** (D3D11-supported); compile generated PS with `D3DCOMPILE_DEBUG` / `/Zi` so symbols are present |
| Capture diagnostics under explorer launch | `DEBUG_REPORT_LOG_PRINT` (goes to OutputDebugString/stdout — invisible on smokes) | `ID3D11InfoQueue::AddApplicationMessage` -> drained to `stage/d3d11-debug.log`; or direct file I/O (the `iter18-stage0-resolve.txt` pattern); attach DebugView for OutputDebugString |
| Validate runtime correctness | Trust exit code | Watch the InfoQueue error classes: `id=342` (input subset), `id=343` (register position), `id=353` (SRV slot unbound), `id=281` (format-castable) |
| Confirm a fix didn't regress D3D9 | Only test D3D11 | Boot **both** `rasterMajor=5` and `=11` to the same scene; the project invariant requires it |

## "Looks Done But Isn't" Checklist

Things that appear complete on a D3D11 smoke but fail parity against D3D9.

- [ ] **Textures appear:** Often missing material/lighting/multi-stage math — verify eyes aren't gray, minimap is round, specular/glow present, A/B against D3D9.
- [ ] **Geometry renders:** Often a matrix-transpose or single-stream-skinning issue lurking — verify skeletal characters aren't sharded and a new cbuffer matrix is transposed.
- [ ] **Gamma "looks fine":** Often the LUT is missing (too bright) OR double-applied (washed) — verify the post-process LUT runs once and matches D3D9 interior haze.
- [ ] **Load screen shows:** Often the half-texel seam at center — verify no seam across multiple splash images.
- [ ] **UI/fonts render:** Often a global blend-default masking a per-pass bug — verify blend is per-pass, not globally on; verify font crispness (half-pixel).
- [ ] **No crash in a quick smoke:** Often the c0000005 single-stream path or the pre-existing 0x087a — verify with an NPC-heavy mouse-over loop; don't conflate the two.
- [ ] **InfoQueue "quiet enough":** Often residual id=342/343/353 — verify exact zero on the linkage classes after PS-generator changes.
- [ ] **Particles/ribbons appear:** Often wrong blend mode / premultiplied-alpha / sort — verify additive vs alpha-over matches D3D9, and ribbon trails aren't black-boxed.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Gamma double-correction | LOW | Remove the redundant correction (revert sRGB format flip); keep only the single LUT post-process; A/B interior vs D3D9 |
| Half-texel seam | LOW | Centralise the half-pixel offset in the viewport/screen-space transform; remove per-draw fudges; re-test multiple splashes |
| Forgotten matrix transpose | LOW | Add `XMMatrixTranspose` at the cbuffer store for the offending matrix; verify clip.w sign |
| Wired state to dead `ShaderImplementationData::apply` | LOW | Move the wiring to `StaticShaderData::apply(passNumber)`; re-smoke |
| "Fixed" the Compare swap | LOW (if caught fast) | Restore the swap to mirror D3D9; A/B foliage/decals/transparency |
| Re-enabled blend factors prematurely | LOW | Revert (as Iter-44C was); defer until multi-stage PS sampling + material constants land |
| Regressed the D3D9 path via shared-code edit | MEDIUM | `git` bisect the shared edit; gate behind the plugin or guard with renderer check; re-establish dual-renderer boot |
| Single-stream skeletal shards | MEDIUM-HIGH | Choose: enable multi-stream caps (`Direct3d11_VertexBufferVectorData`) OR fix single-stream skinning math; RenderDoc mesh-viewer verify |
| Shader permutation explosion / state-object churn | MEDIUM | Add disk `.cso` cache + zero-init descriptors; instrument create counts |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls. (Phases continue from 16; v2.2 starts at **Phase 17**. Names below are the COMPARISON.md gap buckets / milestone target features — the roadmapper assigns final numbers.)

| Pitfall | Prevention Phase(s) | Verification |
|---------|---------------------|--------------|
| 1. Gamma double-correction / flat-white | Gamma-LUT + lighting (after asset-PS) | Single LUT post-process; interior A/B vs D3D9; no sRGB format flips |
| 2. Half-texel seam | Load-screen / 2D-blit (early, isolated canary) | No center seam across multiple splashes; HUD text crispness matches D3D9 |
| 3. Matrix transpose dual | Cross-cutting; esp. skeletal/eyes + any cbuffer-matrix work | clip.w sign check; new matrix renders correctly |
| 4. PS-VS linkage contract | Asset-PS pipeline (#1) | `id=342 == 0 && id=343 == 0` in d3d11-debug.log |
| 5. Magenta + flat-white (one bug) | Asset-PS pipeline + multi-stage sampling (eyes, minimap as cases) | Eyes correct, minimap round, residual magenta tracked; D3D9 A/B |
| 6. Render-state defaults divergence | Asset-PS pipeline; revisited in particles/ribbon | Per-pass state via `StaticShaderData::apply`; eye/decal/foliage A/B; state-object create count stable |
| 7. "Fixing" the Compare swap | Cross-cutting guard (asset-PS, particles) | Compare-table edits gated on D3D9 A/B; swap documented inline |
| 8. Single-stream skeletal shards | Skeletal/eyes (char-select beachhead) | RenderDoc mesh-viewer A/B of a skinned draw; settled-frame re-capture scopes static-mesh question |
| 9. Missing material/light/fog constants | Asset-PS pipeline + gamma/lighting | Per-material A/B; cbuffer contents in RenderDoc |
| Permutation explosion / state churn | Folds into asset-PS pipeline; matters at world-streaming scale | Disk `.cso` cache; zero-init descriptors; create-count metrics |
| Exterior static-mesh distortion (OPEN) | Geometry-distortion investigation phase | Re-capture settled exterior frames; determine skeletal vs static; RenderDoc |
| DPVS D3D11 remeasure | DPVS remeasure (deferred SPEC R7, "after asset-PS" = now) | Stable visible geometry to instrument; PIX/Nsight/RenderDoc timing |

**Suggested phase ordering (from COMPARISON.md + dependency analysis):**
1. **Asset-PS pipeline** (#1 blocker) on **character-select** — unblocks textures, magenta, and feeds lighting + minimap. Owns Pitfalls 4, 5, 6 (state), 9 (constants).
2. **Load-screen seam** — small, isolated, deterministic; sampling-path canary. Owns Pitfall 2. (Can run in parallel; no dependency on #1.)
3. **Gamma/LUT + lighting** — after #1 so you correct a correctly-shaded image. Owns Pitfall 1.
4. **Skeletal/eyes + multi-stage sampling** — char-select beachhead completion. Owns Pitfalls 8, 5 (eyes), 3 (bone matrices).
5. **Extend PS pipeline to open world** — permutation/cache scale concerns surface here.
6. **Geometry-distortion investigation** (static-mesh shards, OPEN) + **particles/ribbon** + **minimap** as it rides the multi-stage work.
7. **DPVS D3D11 remeasure** — last, needs stable visible geometry.

## Sources

- **This engine's own iteration record (HIGH):** Phase 11 memory notes — `phase11-x4016-overlapping-registers`, `phase11-font-rendering-resolved`, `d3d11-cbuffer-transpose-quirk`, `d3d11-baked-rt-bgra-format`, `d3d9-compare-table-greater-equal-not-equal-swap`, `project-d3d11-collide-use-after-free`, `project-phase11-minimap-never-round`, `project-phase12-visual-baseline`, `feedback_dont_modify_koogie_changes`.
- **Visual baseline (HIGH):** `docs/research/phase12-baseline/COMPARISON.md` (5 gap buckets, matched D3D9/D3D11 pairs).
- **Live plugin source reads (HIGH):** `Direct3d11_StaticShaderData.cpp` (per-pass apply, state, sampler/stage), `Direct3d11_PixelShaderProgramData.cpp` (dynamic PS generator), `Direct3d11_StateCache.cpp` (viewport c9, state-object cache, gamma), `Direct3d11_Device.cpp` (backbuffer format), `Direct3d11_TextureData.cpp` (format table, BC pad), `Direct3d11_VertexBufferDescriptorMap.cpp` (BGRA color ordering); D3D9 siblings `Direct3d9.cpp` (`setBrightnessContrastGamma` LUT), `Direct3d9_StateCache.cpp` (`D3DSAMP_SRGBTEXTURE=0`), `Direct3d9_ShaderImplementationData.cpp` (Compare table).
- **D3D9->D3D10/11 semantics (HIGH, cross-checked against source):** half-texel -0.5 rule removed in D3D10+ (pixel-center convention change); no swapchain `SetGammaRamp` in DXGI; `D3D11_FATAL_INPUT_SUBSET` linkage contract; immutable state objects + no FFP alpha-test.
- **RenderDoc D3D11 capability (HIGH):** [How do I debug a shader? — RenderDoc docs](https://renderdoc.org/docs/how/how_debug_shader.html), [Features — RenderDoc docs](https://renderdoc.org/docs/getting_started/features.html) — D3D11 vertex/pixel/compute shader debugging, mesh viewer, texture/RT inspection; `D3DCOMPILE_DEBUG`/`/Zi` required for symbols.

---
*Pitfalls research for: D3D9->D3D11 asset-shader + visual-parity porting (SWG/whitengold client, v2.2 Visual Parity)*
*Researched: 2026-05-27*
