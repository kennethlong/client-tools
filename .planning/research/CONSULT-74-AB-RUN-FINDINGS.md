# CONSULT-74 A/B run — findings (2026-08-14)

Live dual-client session: both clients against the same stock server (192.168.1.200:44453),
coordinate-matched scenes, RenderDoc captures. Repo A = ours (`f5f14c3f6`), Repo B = Sais
(`x64-dx11-vanilla` line, packaged squash build). Full protocol:
[CONSULT-74-AB-RUN-PROTOCOL.md](CONSULT-74-AB-RUN-PROTOCOL.md).

## The 2×2 result (completed live)

| | Our data (SWGSource v3.0 stack) | His data (`_client_dx11`, retail-style layout) |
| --- | --- | --- |
| **Our client** | correct (baseline) | **correct** — char select clothed, cantina lighting right |
| **His client** | char select naked; in-world clothed only via his retry net; cantina whitewashed | (his baseline — not re-run locally yet) |

Ours renders correctly on both data sets. His mis-renders on ours. Data exonerated both directions.

## Conviction: the cantina whitewash (capture-diffed, instruction-level)

Captures: `stage-x64/Capture201.rdc` (ours) vs `stage-B-x64/Capture201.rdc` (his), same cantina
spot (~3457 4 -4826). Framebuffer diff: **80.5% of pixels differ** (diff image:
`CONSULT-74-cantina-diff.png`). Exported frames: ours = correct dim interior; his = same room at
near-full brightness, structure/textures/NPCs all fine.

Pixel history on his wall (1300,400) → written by draw 248, shader-out ≈ (0.84, 0.76, 0.59).
Pixel debug: **PS COLOR input v1 = (1.223, 1.230, 1.254, 1.85) — above 1.0.** VS disassembly
(hash bc7d0e89…, his converted corpus):

```
4:  add r0, v2, c[16]     ; baked vertex color + scene ambient  <- his c_ambient patch (mov->add)
5:  add r0, r0, c[61]     ; second additive boost
    ...7 dynamic lights mad-accumulate...
92: add o1, r0, c[14]     ; third additive term; output unclamped
```

Stock semantics: for precalc-vertex-lit interiors the baked vertex color IS the lighting
(`mov r7, vColor0`), dynamic lights add on top. His corpus replaces the mov with adds + boosts.
On his (broken) data the vertex streams came up zero → black walls → he added ambient to survive.
On correct data the baked ~0.2 lighting gets ~1.0 added → every interior wall at ~120% brightness.

**Same root cause, three symptoms, now proven end-to-end:** broken x64 TRE/TOC layer → zeroed
vertex data → shader-level compensations (c_ambient add, skinned ARGB==0→white rewrite, 0.3/0.85
floors) → mis-render on well-formed data. This confirms adversarial-close item 5/6 empirically.

**Consequence for the merge: his D3D11 renderer core is exonerated.** The whitewash lives in his
conversion corpus + compensation patches (data-side, deletable), not in the renderer architecture.
Fix path = fix/replace his data layering, then REMOVE the compensations — not rewrite the renderer.

## Other findings this session

1. **His x64 D3D9 renderers have never loaded.** `Gl_api` 864 vs 896 bytes — his exe builds
   `PRODUCTION == 0` (the `#if PRODUCTION == 0` block in Graphics.h:307 = 4 ptrs = 32 bytes), his
   `Direct3d9.vcxproj` Release|x64 builds `DEBUG_LEVEL=0`/`PRODUCTION != 0`. Deterministic
   (rebuild reproduced byte-identical DLLs). His own load-time guard caught it — that guard is
   worth adopting regardless. Consequence: he has had NO x64 D3D9 reference, which explains both
   the data-workaround pattern (nothing to diff against) and his plan to delete D3D9.
2. **Sais confirmed (Discord): he never used the stock SWG-Source TRE set** on the x64/DX11 line
   ("i could never get their tre stuff working down the x64 line"). His cfg's own generated header
   documents the direct-mount workaround: 65 TREE0005 roots mounted directly because his exe
   "direct-loads TREE0005 only; TREE0006 fatal as searchTree" — TOCs cover only the TREE0006
   subset. Suspicion (untested): his x64 TOC resolution is broken, which would be the true root
   under everything above.
3. **Char-select wearables: real regression in his tree on every data set** (naked at char select
   on complete data; in-world dressing works only via his `verifyWornItems` retry net). Localized
   to the char-select avatar path; his container attach path is stock.
4. **His screenshot key is dead** — handler provably never runs (no `screenshots/` dir created);
   detection (DIK_SYSRQ), wiring (`ClientMain.cpp:318`), keyboard path, and `createDirectories`
   all stock and identical to ours; works on ours on the same machine/data. Cause unknown — needs
   live debug with his symbols. Worked around via RenderDoc RT export.
5. **Merge agreement #1 (Discord, Sais): "go for it — i stopped messing with 9 personally."**
   D3D9 ownership transfers to us; our x64 gl05 verified live today (boots, renders, clothed).
   Retires the port-plan P5 threat: D3DX-free is achievable with gl05 alive (ours proves it), and
   the D3D9 line has a maintainer.
6. **Staging corrections made during the run** (both were my errors, recorded for honesty):
   REV1 of his A/B cfg was TOC-only (missing his 65 direct mounts) — REV2 mirrors his layering
   exactly; and his packaged squash ships WITHOUT the asm2hlsl corpus (lives only on his branches /
   client-assets) — extracted from `origin/x64-dx11-vanilla` to `stage-B-override/`.

## Still open

- Outdoor pair (`Capture200`) not yet diffed — indoor mechanism was the priority. Expect the same
  additive signature outdoors (his exterior looked "different texture" to Kenny; less diagnostic
  because exteriors are dominated by dynamic sun, not baked color).
- The COLOR-clamp question for OUR renderer (his C35 find) — his capture can't answer it; needs a
  scene where OUR output exceeds 1.0. Unresolved; still worth the cheap central saturate.
- His vanilla TRE set (offered) — for the tre-compare three-way manifest.
- Scene 4 (JTL space) and scene 5 (timed zone-in) not yet run.

## Message for Sais (draft points, friendly)

1. Your D3D11 renderer core is fine — we captured it rendering our data and everything except
   lighting is correct.
2. The interior brightness is your `c_ambient` add + boosts compensating for vertex data your
   x64 data layer wasn't loading; on well-formed data they over-brighten. Instruction-level
   trace available.
3. Your x64 gl05/06/07 have never been able to load: exe `PRODUCTION==0` vs plugin `DEBUG_LEVEL=0`
   → `Gl_api` 896 vs 864. One-line project fix; your own guard's message names it.
4. Char select renders naked in your tree even on complete data (in-world is saved by your retry
   net) — separate path, worth a look.
5. Your packaged repo ships without the asm2hlsl corpus — anyone building from it under-renders.
6. We'll take D3D9 per your note — ours is x64-working and D3DX-free.

## Outdoor pair (Capture200) — the "over-bloomed" sky, convicted (2026-08-14 evening)

Matched-pixel comparison (same coordinates both captures, cameras framed differently — content
matched by region, not raw x/y):

- **Upper sky: near-parity.** Pure-sky pixel (1250,50): ours (0.376, 0.290, 0.255) vs his
  (0.412, 0.373, 0.361). His ~10-30% brighter — consistent with his global ambient adds — but the
  same dusk-purple ballpark. NOT the dramatic difference.
- **The horizon glow band is the blowout, and it is arithmetic over-range, not bloom.** His sky
  pixel (900,120): correct dusk purple (0.298) through the whole world pass, then **draw 3755** —
  a fullscreen celestial composite from his converted corpus (PS: `sample diffuseMap; sample
  smallMap; mad r0 = smallMap * smallMap.a + diffuseMap` + his SwgPixelEpilogue fog) — outputs
  **(1.1196, 0.69, 0.89, alpha=2.0)**, clipped to 1.0 on write. A large band of horizon sky
  saturates at the clip ceiling → the "over-bloomed" look. His vanilla branch has NO bloom
  post-process; the bloom look IS clipping.
- Ours composites the equivalent content to ~0.3-0.7 in the same regions, no clipping anywhere.
- **⚠ DATASET CORRECTION (caught by Kenny 2026-08-14 evening): the captures were CROSS-dataset,
  not same-data.** At capture time our client pointed at HIS `_client_dx11` set and his client
  pointed at OUR `SWGSource Client v3.0` set (both verified from the cfgs). This means: (a) the
  80.5% indoor pixel-diff number is not a controlled same-data comparison — carry that asterisk;
  (b) the mechanism convictions are UNAFFECTED and actually STRENGTHENED — his renderer
  over-brightened canonical data (no data excuse), ours stayed correct on his data (cross-validated
  both directions); (c) the two sets share provenance anyway — his cfg header records
  `_client_dx11` was generated FROM "SWGSource Client v3.0", so underlying TRE content is the same
  distribution re-laid-out. For an airtight same-data capture pair, re-point one cfg and recapture
  one scene.
- The alpha=2.0 (two layers each contributing ~1.0 alpha, summed) plus the >1 color sum point at
  the composite ADDing where the reference semantics effectively modulate/blend — i.e. a
  **conversion-semantics defect in the celestial composite shader**, same family as the indoor
  `c_ambient` mov→add. Whether it is the same asm2hlsl translation rule or a per-shader patch is
  not yet established (his corpus file for this shader not yet identified by name).

**Answer to "was it intentional?": no, on the evidence.** Intentional style would live below the
clip ceiling; his horizon is pinned AT 1.0 with over-range inputs (1.12, alpha 2.0) — that is
arithmetic saturation, not grading. The mild global brightening (~10-30%) IS his deliberate
compensation layer (ambient adds), but the horizon blowout is a defect in the converted shader.

Correction to an in-session note: draws 3702-3738 rendering to another target were NOT evidence of
a bloom pipeline (no bloom exists in his vanilla line); they are other-target draws (UI/composite
chain), nothing more.

## LIVE-VERIFIED (2026-08-14 evening): two-file fix renders his cantina correctly

Kenny confirmed on Sais's own client, same cantina spot, after restart:
1. **Interior fog RESTORED** — `stage-B-override/datatables/interior/interior.iff` (our merged
   table, `8cd8c2d82`) at priority 12 beating raw ILM's fog-off at 5.
2. **Whitewash GONE** — `c_ambient.inc` reverted to stock cell semantics (`r7 = vColor0;`,
   original preserved as `.sais-orig`). The c_ambient conviction is upgraded from
   instruction-level-traced to LIVE-VERIFIED. The tangentColor flat add (4 diffuse*.inc sites)
   was NOT needed for this result; park unless residual brightness vs our reference shows up in
   the capture diff.
3. Expected residual: skinned NPC brightness (ARGB==0->white rewrite is compiled into his engine,
   unreachable from loose files).

**Char-select naked: reclassified.** Clothes reappeared after the restart with changes that
cannot affect wearables; no shader cache exists in his stage (no shaderCacheDir -> fresh compile
every launch), killing the cache-invalidation theory. Best-fit mechanism: the SAME async
skeletal-appearance load race his own in-world `verifyWornItems` retry exists to paper over —
char select has NO retry, so the outcome flips with cold/warm OS file cache (first run of the
day naked, warmed-up runs dressed). Downgrade from "deterministic regression" to
"load-timing race, char-select path, no retry" — intermittent, which is worse to debug blind.
Cold-boot repro available if ever needed.

**Demo artifact for Sais:** his renderer + his corpus + two-file diff = correct cantina. The
message can now LEAD with his renderer working, not with what was wrong.

## RETRACTION + methodology fix (2026-08-14 evening, caught by Kenny)

**RETRACTED: "ours is missing the tent canvas / parts float in air / canvas never submitted."**
All three observations were artifacts of exporting MID-FRAME. The outdoor exports were taken at
draw 8800/14639 (ours) and 3812/4346 (his) — after the opaque pass, BEFORE the sorted-alpha pass
that draws translucent fabric (tent canvas) and other late geometry. The true-final ours frame
(eid 14639) shows a complete scene: canvas edge present, nothing floating, full UI. The
pixel-history probes that "proved" no draw touched the canvas pixels were bounded at eid 8829 —
before the alpha pass — and prove nothing. Our renderer is fine; Kenny's memory of the live game
was correct.

**Root cause of the method error:** MCP goto_event probing with round-number eids silently missed
the tail of the draw list twice (non-draw eids return "not found", indistinguishable from
"past the end"). RULE GOING FORWARD: always take the last-draw eid from `renderdoc-cli draws`
(authoritative full list) before exporting or bounding pixel history. Pics folder rebuilt from
true-final frames: mos-eisley-OURS (14639), mos-eisley-SAIS (4346), cantina-SAIS (5861);
cantina-OURS (13838) and cantina-SAIS-FIXED (6822) were already true-final.

**Still open from Kenny's observation set:** the roofline glow trim (his showed bright parapet
strips, ours modest) — re-evaluate against the TRUE-final pair before treating as a finding; his
mid-frame export may have exaggerated it and/or his ambient boost accounts for the rest.
The live prepareToView() failures in OUR log (belt_s14/armor_zam, CustomizableShaderTemplate
TextureFactorIntOperation) stand — B's `e961a57c5` fix observed firing in our own client.

## A/B: c_ambient vs char-select nakedness — FALSIFIED (2026-08-14 evening)

Test: restored HIS original c_ambient.inc (vColor0 + ambient), fog fix kept, warm cache, restart,
check char select. Result: **CLOTHED.** The ambient add does NOT cause the naked char select; the
post-edit clothes were coincidence. The alpha inflation (COLOR.a = 1.85 measured) is real but not
visibility-breaking on that path. Standing theory reverts to: async skeletal-appearance load race
on the char-select path (no retry), flips with cold/warm cache. Full confirmation someday = naked
on a cold first-launch-after-reboot. Patched module restored after the test.
Design note kept regardless: any ambientBoost knob must boost COLOR ONLY and pin alpha
(r7.a = vColor0.a) — the measured 1.85 alpha is a latent hazard even if asymptomatic here.

## RETRACTION 2 (2026-08-14 night): the horizon "blowout" is STOCK BLOOM, not a conversion defect

Traced draw 3755's PS to source: sampler names `diffuseMap`/`smallMap` exist NOWHERE in his
239-file corpus (which names samplers `pixelSampler0/1`) — they are the naming convention of
data-carried `//hlsl` PSRC. A TRE sweep of our v3.0 set found exactly ONE pixel program
containing `smallMap`: **`pixel_program/2d_bloom.psh`** (ILM_visuals.tre / patch_09:
`return base + (bloom * bloom.a);` — instruction-identical to the captured
`mad r0 = smallMap*smallMap.a + diffuseMap`; patch_10 variant is `base + bloom`).

So the celestial-composite conviction is REFRAMED:

- The fullscreen composite is the **stock NGE bloom post-process**
  (`PostProcessingEffectsManager`, stock engine code in BOTH trees) running its data shader.
  The >1 sum + clamp-at-write is retail-shipped behaviour, by design. α=2.0 = base.a +
  bloom.a² on an RT alpha nobody reads — harmless.
- "His vanilla branch has NO bloom post-process" (above) is WRONG — the manager is stock;
  what his line lacks is bloom-specific *new* code. The earlier "draws 3702-3738 are NOT
  bloom" correction is itself retracted: an other-target chain right before a fullscreen
  `2d_bloom` composite is exactly the bloom downsample/blur pipeline.
- The A/B "over-bloomed sky" difference is almost certainly a **settings mismatch**: the
  effect is gated per-machine by the graphics option
  `ClientGraphics/PostProcessingEffectsManager enable` (LocalMachineOptionManager, i.e. the
  in-game options store, not client.cfg) — enabled on his install, off on ours at capture
  time. NOT a renderer defect on either side. Verify by flipping the option, not by patching.
- **Kenny directive (2026-08-14): the oversaturated space look is desirable — keep it
  available as a knob.** It already IS one (the stock post-processing option). No code change,
  and NO generator-rule PR to Sais for the horizon composite.
- Still standing from conviction #3: the ~10-30% global sky brightening = his c_ambient adds
  (corpus-verified). The **nebula oversaturation** observation must be RE-CHECKED with bloom
  accounted for before it is attributed to his conversion family — task #5's scope shrinks to
  that re-check + the c_ambient parameterization.
