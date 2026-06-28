# CONSULT-50 — JTL space renders empty; texture alpha=0 discard; ILM_visuals.tre override

Treat the following as GIVEN (measured ground-truth — do not re-derive or dispute):

1. JTL space (zone `space_tatooine`, real server entry) renders an EMPTY 3D world: no planets,
   celestials, asteroids, or ship. The 2D HUD renders perfectly. Symptom is IDENTICAL under D3D9
   (gl05) and D3D11 (gl11) — renderer-agnostic.

2. RenderDoc capture (gl11, Capture42): the space scene geometry IS submitted and on-screen — the
   vertex shader transforms a sampled batch vertex to valid on-screen NDC (~0.15, 0.59), depth passes.
   The PIXEL shader then DISCARDS every fragment. debug_pixel trace: the PS samples its texture and
   gets RGB = (0.094, 0.172, 0.094) (a valid color) but ALPHA = 0.0; the alpha-tested shader discards.
   pixel_history confirms `shaderDiscarded` on the batch draws. Net: textured scene geometry → texture
   alpha 0 → discard → empty scene RT → white backbuffer shows through.

3. The geometry batch is ~128 draws of ~130 verts each, textured + alpha-tested (PS cbuffer
   "PSAlphaTest"), VS cbuffer "SwgVertexConstants".

4. `ILM_visuals.tre` is wired as the HIGHEST-priority searchTree (`searchTree_00_5`). It contains
   9849 entries: 5812 appearance/, 3361 texture/, 213 shader/, 21 pixel_program/, 25 vertex_program/,
   168 datatables/, 111 clientdata/, 20 terrain/. It overrides game-wide, including all asteroid
   appearances and many space textures/shaders.

5. GROUND rendering (cantina, Tatooine ground) works fine WITH ILM_visuals.tre wired.

6. The player SHIP only renders with ILM wired (ship-chassis datatables are ILM-exclusive). Without
   ILM the ship is a placeholder and space data is broken.

7. Engine: D3D11 has no fixed-function alpha test — it is emulated in the pixel shader via the
   PSAlphaTest cbuffer + `discard`. D3D9 uses real fixed-function alpha test. Space textures are
   referenced from `terrain/space_tatooine.trn` (skybox `nebula2`, `texture/env_space_tato.dds`,
   `terrain/colorramp/stars_tato.tga`, celestial shaders `shader/cels_star_back.sht` /
   `starglow_radial.sht`, planet appearances `appearance/planet_tatooine*.pln`).

## Question
Two independent angles wanted (do NOT converge — split):

- Code-path angle: In this engine, how is a texture's binary selected when multiple search trees
  contain the same logical name (priority/override semantics)? Trace where a loaded DDS/texture's
  alpha channel could end up 0 at sample time (format/decode path, DXT1-vs-DXT5, BGRX-vs-BGRA,
  swizzle). Separately, where/how is alpha-test enabled for space scene geometry, and could an
  override change which shader/alpha-ref is used?

- Asset/override angle: What is the most likely mechanism by which adding a high-priority asset TRE
  (textures + shaders + shader-programs) makes previously-working geometry sample alpha 0 in BOTH
  renderers? What single shared resource, if shadowed by a bad/alpha-stripped/format-changed copy,
  best explains a WHOLE space scene discarding (not just one object)?
