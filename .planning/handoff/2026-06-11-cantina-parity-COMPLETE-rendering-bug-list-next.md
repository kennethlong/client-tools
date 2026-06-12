# Handoff 2026-06-11 (evening) — Cantina interior parity COMPLETE; pick up the rendering bug list next

Branch: `koogie-msvc-cpp20-base` (THE renderer checkout, D:\Code\swg-client-v2). Build the plugin with
`& $env:MSBUILD src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj -p:Configuration=Release -p:Platform=Win32 -nologo -m -nodeReuse:false -v:minimal`
(postbuild auto-stages gl11_r.dll). Boot `SwgClient_r.exe` (client.cfg, rasterMajor=11), FULL restart to load a new
DLL or shader overrides. Staged `gl11_r.dll` md5 **98d82ea9**. RenderDoc MCP fully operational (open_capture /
pixel_history on the SCENE RT ::323 not the backbuffer / debug_pixel trace / export_texture / export_render_target).

## COMMITTED TODAY (all on koogie, not pushed)
- **738d69e6f** fix(d3d11): FFP wall textures (cascade gate + pass->m_stage SRV binding + srv0Bound dual variant),
  point-light VS feed, sticky dot3 (CONSULT-42), alpha-fade (CONSULT-40).
- **f34282592** feat(d3d11): FFP combiner-cascade PS (full D3D9 texture-stage emulation, TFACTOR in b1) +
  per-pixel fog in ALL THREE PS lanes (legacy generated, FFP cascade, 17-07 asset wrapper via cbuffer PSFog1707@b1)
  + the VS fog constant c10={0,0,density,density²} that was NEVER fed ("zero for now") + first-component sort
  tie-break for shared-register VS outputs (generators + 17-07 rewriter).
- **cb2b16c1a** feat(d3d11): stage/override now TRACKED (.gitignore stage/* + !stage/override/; .gitattributes pins
  *.psh binary). Payload: 9 .vsh reauthors (tfcl/tfcsl family + NEW c_simple + NEW envmask_specmap_c), 5 .psh PSRC
  overrides (texren face-tint pair + NEW emissive trio emismap/emis_full/a_emis), reusable IFF rebuilder
  `_build_emis_overrides.py`.

## VERIFIED THIS SESSION (Kenny, screenshots 0361/0362/0363 + Captures 37/38)
Cantina interior at D3D9 parity: textured detail-blended walls/floor, full-scene haze (all lanes fogged), bar/canopy/
cistern lit+fogged (c_simple/envmask VS overrides), bar under-counter emissive glow at EXACT parity (measured),
wall-light fixtures no longer magenta, particles fixed. "Purple shoes" investigated end-to-end = NOT a bug —
a customization-tinted NPC rendered CORRECTLY for the first time (the tie-break fix lets real color2 asset PSes bind
where the tint-ignoring fallback used to; expect MORE color variety on NPCs now — that's correct).
Far-field/fog-thickness deltas measured at parity (PIL pixel sampling + capture arbiters; earlier "thinner fog /
too bright" impressions were unfogged-asset-lane draws, since fixed, plus screenshot-framing artifacts).

## GOTCHAS LEARNED (cost real time — don't relearn)
- VS override compile failure FATALs the client (PS lane tombstones). Validate constant names against the runtime
  reflection (stage/stage/vs-disasm-all.txt) NOT the vsh-extract headers (older revision: it says `cameraPosition`,
  runtime is `cameraPosition_w`). Crashed 2x before the fix.
- RenderDoc has NO D3D9 support (the old "capture-and-diff gl05" idea is impossible; apitrace could if ever needed).
- Scene-RT PNG exports show characters as WHITE BLOBS — alpha-channel artifact (RT alpha holds bloom), not a bug.
- PowerShell 5.1 mangles embedded double-quotes passed to git -m; use `git commit -F <file>`.
- The armor_marauder belt .mgn async-load crash (c0000005, retry succeeds) is PRE-EXISTING — don't chase as a
  regression (it fired right after an override change and looked related; it wasn't).

## THE RENDERING BUG LIST (updated 2026-06-11 late — Kenny confirmed several already fixed)
1. ~~Camera-locked prop~~ — **FIXED, Kenny-confirmed 2026-06-11.**
2. ~~Precalc-vertex-lit BUILDINGS (exterior)~~ — **LOOKS FIXED per MCP-driven sweep 2026-06-11 (late)**:
   walked Mos Eisley in-client (D3D11, gl11_r 98d82ea9), 360° panorama (stage/screenshots/mcp_pano_1..4.png) —
   domed buildings textured + directionally shaded (lit/shade sides on domes), zero black or flat-white buildings.
   Kenny eyeball to close formally.
2b. ~~Dark terrain patches~~ — **FIXED 2026-06-12, Kenny runtime-VERIFIED ("black edges gone, smooth transition")**.
   TWO stacked bugs:
   (i) commit 44ade0ae4 — away-facing slopes black: tfcl //hlsl reauthors used plain max(N·L,0)·diffuse; the asm
   sun is HEMISPHERIC via cExtLtData c60-63 (facing=diffuse, terminator=tangent, away=back). Fix =
   calculateDiffuseLightingHemi in the 4 tfcl* overrides (c60-63 already fed by composeSlot0Shadow). GOTCHA
   re-bitten: runtime constants.inc member is direction_w, not direction — FATALed once.
   (ii) commit fcfbb2226 — THE main one (Capture39 pixel trace events 903/927): terrain layers = ADDITIVE pass
   pairs (pass0 = layer0·(1−α) pre-darkened, pass1 = layer1·α, blend One/One/Add per the .sht pass DATA), but
   D3D11 only ever wired blend ENABLE — factors stuck at default SrcAlpha/InvSrcAlpha, and the terrain PS packs
   BLOOM (≈0) in o0.a → pass1 vanished → pre-darkened base showed = the hard-edged patches. Fix = the Iter-44C
   re-land: per-pass setAlphaBlendFactors from pass data (+ _COLOR→_ALPHA mapping on the alpha side). This applies
   pass blend factors EVERYWHERE now (particles/glow/UI) — watch for blend regressions in other scenes.
3. ~~Eyes / baked detail layers~~ — **FIXED a while back, Kenny-confirmed 2026-06-11** (texren mt_lerp family;
   override recipe is in cb2b16c1a's payload lineage).
4. ~~Furniture LOD-thrash flicker~~ — **FIXED, Kenny-confirmed 2026-06-11.**
5. ~~FFP cascade quality pass~~ — **CLOSED "done for now" per Kenny 2026-06-11 (late)**: known-approximate by
   design (env-map stages sample .xy of reflection vector as Texture2D, CG_scroll1/2 static, TA_specular falls
   back single-texture); accepted as good enough. Reopen only if a specific surface looks visibly wrong.
6. ~~gradient_sky + cloudlayer fallback VS~~ — **FIXED, Kenny-confirmed 2026-06-11.**
7. ~~GAMMA-01 plumbing~~ — **FIXED 2026-06-11 (late), commit 493287510**: setBrightnessContrastGamma now real —
   D3D9's exact ramp curve (Direct3d9.cpp:2207) as a pre-Present fullscreen pass over the backbuffer in
   Direct3d11_Device (copy→temp SRV→curve PS→backbuffer; identity = pass fully off, zero cost; raw state
   save/restore keeps StateCache truthful). Runtime-verified: gamma 0.25 via local_machine_options.iff crushes
   mean RGB 85/70/56→0.4/1.6/2.4 (login A/B mcp_cap_019 vs mcp_cap_020). Options sliders should now work —
   Kenny eyeball welcome.
8. ~~Mini-map never round~~ — **FIXED 2026-06-11 (late), commit e4f94a0f6**: ui_radar.psh ps.1.1 PS (alpha =
   ui_radar_mask.dds circular mask on s1/TEXCOORD1) tombstoned → fallback dropped the mask → square. //hlsl PSRC
   override (texren recipe, _build_ui_radar_override.py); ui_radar.vsh already //hlsl, untouched. Verified round
   in-client via MCP-driven restart (stage/screenshots/mcp_radar_crop.png). Kenny eyeball to close formally.
   Parked non-rendering: ~~no music / no in-game audio~~ — **FIXED 2026-06-12, Kenny-verified** (stage/miles/
   Miles codec redist dir was missing — mssmp3.asi + 3D providers; copied from "SWGSource Client v3.0/miles";
   runtime file only, no commit; see project_audio_fixed_missing_miles_redist memory). ~~Warning-flood perf drag~~
   — **ALSO FIXED 2026-06-12, same root cause**: the flood was 5327× "Unable to create a valid sample id"
   (+12-line callstacks each) from failed sound creation; post-fix census 141k→1.8k log lines, 0 sample-id
   warnings, Kenny: "way less laggy". THE ENTIRE 2026-05-25 PARKED LIST AND THE RENDERING BUG LIST ARE NOW EMPTY.

NOTE: items 2/5/7/8 carry "not sure, might have already been worked" uncertainty (Kenny, 2026-06-11). Before
designing a fix for ANY of them, do a cheap visual/runtime check first — the list above predates several sessions.

## KEY ARCHITECTURE NOTES FOR THE NEXT SESSION
- Three PS lanes: native asset PS (rare; param-less only) → 17-07 rewritten asset PS (the workhorse; VS-mirror
  wrapper struct + fog) → generated fallback (selectFallbackPSForVS: FFP cascade desc > srv0Bound tex*color >
  color-only > magenta). FFP combiner desc flows construct→setFfpCombiner→getOrCompilePSForVS keyed
  (vsHash, ffpHash, srv0Bound).
- b1 cbuffer = Direct3d11_PSAlphaTestCB {alphaTest, textureFactor, fogColor} 48B, shadow with -1 sentinels in
  StateCache, bound every draw in applyPreDrawState.
- Fog: VS c10 fed by setFog; per-pass color mode setPsFogMode (Normal/Black/White); all generated/wrapped PSes lerp
  post-alpha-test using the VS FOG output.
- Memory files carry the full detail: project_d3d11_ffp_combiner_cascade (this week's mega-entry),
  project_d3d11_ffp_walls_untextured_fix, project_d3d11_purple_boots_capture38 (incl. the reusable capture
  diagnostic recipe), project_d3d11_blue_face_texren_bake_tint.
