# Sais single-PR queue (branch strict-data-defaults, PR #1 DRAFT)

**Restore-knobs ledger: [SAIS-KNOBS.md](SAIS-KNOBS.md)** — every hard-coded behavior of his
the branch removed/gated + the exact cfg key or file copy that puts it back. Fold into the
PR #1 body before reopening.

Delivery model: ONE PR, clean atomic commits, Sais reviews before merge (memory:
feedback_sais_one_pr_clean_commits). Repo: Galaxies-Reborn/swg-source-x64-dx11 (we have admin).
Cross-repo picks: remote `kenny` = D:/Code/swg-client-v2 in his clone. STRIP `.planning/` from
any cherry-picked commit (our fix commits carry planning docs; `git rm -r --cached .planning
&& rm -rf .planning && git commit --amend --no-edit`).

## Landed (in order)
- [x] c1 76020ceaf engine: strict data by default (+strictData opt-out) — built x64 DX11
- [x] c2 d4b9c5033 Gl_dll.def includes Production.h (root of his never-loading x64 D3D9 864v896) — built x64 All
- [x] c3 DX11 window reveal must activate (no-focus-on-startup; drop SWP_NOACTIVATE) — **LIVE-VERIFIED by Kenny 2026-08-15** (first launch of restaged fixed binaries came up focused; earlier 'still no focus' report was the stale 2:21PM pre-fix stage-B set)
- [x] c4 9c55c9ac5 combat erase-then-++ iterator UB (pick d549a8acd, clean)
- [x] c5 dde0fc82a D3DCREATE_FPU_PRESERVE (pick db040db29, clean; runtime-relevant on Win32 x87)
- [x] c6 77e276fe8 SearchCache leaf mutex + fail-loud zlib (pick 9c03f53c5; planning stripped)
- [x] c7 c3f3669ec CuiMediator::garbageCollect re-entrancy guard (pick 1cfef979b; planning stripped)
- [x] c8 05632b7d5 snapshot mediators in updateMediatorEnabledStates (pick 98f91cb52)
- [x] c9 3437d6d68 HUD handlePerformDeactivate null guard (pick 799660f88; planning stripped)
- [x] c10 a3bfd28e5 reticle null guards (pick 9841ef52c)
- [x] c11 heap-corruption trio (pick f344d1035, clean 5 files)
- [x] c12 CONSULT-56 follow-up: TreeFile snapshot reads, Texture CS, eviction (pick 885b190a0; planning stripped; his TOC-touched TreeFile.cpp merged clean)
- [x] FIELD FIX 2026-08-15: c1's getTable chokepoint FATAL was TOO AGGRESSIVE — killed boot on
      the mahjong layout probe (datatables/minigame/mahjong/test.iff, an optional-content probe
      that never shipped even in v3.0; stock returns NULL, caller skips). Redesigned: strict =
      exact stock NULL return (caller-site STRICT_DATA_FATALs carry the strictness), lenient =
      sentinel, BOTH log a WARNING. Folded into c1 via autosquash rebase (branch SHAs changed;
      force-push pending). LESSON: a fail-fast chokepoint must not FATAL on a probe pattern —
      openIfNotFound=true is used both as "must have" AND "do we have?".
- [x] c13 WorldSnapshot: drain restored + refusal-before-teardown + kill switch +
      REPORT_LOG + detailLevelChanged saveList[i] — batch-built x64 All renderers clean.
      His tree confirmed same lifetime no-op and same refusal-after-teardown hazard;
      stock-ancestor defect, both forks inherited.

## Field session 2026-08-15 (Kenny live, stage-B-x64) — TWO new commits, branch now 15
- [x] c14 f4167a197 skeletal ARGB==0->white rewrite gated behind strictData=false + zero-ARGB
      build WARNING (probe: log proved ZERO hits during a live flash -> rewrite exonerated for
      the flash, gate kept on its own merits)
- [x] c15 1f73947ff DX9 draw-time alpha-fade overrides ported — **LIVE-VERIFIED by Kenny**
      ("faded in as planned, no oversaturation, looked proper"). Root cause of the
      "bloom saturation aura on first sight": fade opacity written into RT ALPHA = the GLOW
      channel (2d_bloom adds bloom*bloom.a); DX9 masks alpha writes + forces blend at draw
      time (Direct3d9.cpp:3953-3961), his port never carried it. Fix = per-pass fade-variant
      cached blend state, selected PER DRAW in prepareToDraw (per-primitive fade vs
      per-shader-group apply — apply-time selection would leak across objects).
      Bonus: his client gains actual translucent fade-ins (objects previously rendered
      opaque while computing an opacity nothing consumed).
- Also live-verified this session: c3 focus fix; strict-data getTable redesign (boot FATAL on
  mahjong probe -> strict=stock NULL; folded into c1).
- Diagnosis lessons: the flash is capture-proof (RenderDoc overhead + low fps both feed the
  loader -> heisenbug); frameRateLimit walk-up (10/30/uncapped) established fps-scaling;
  conviction by code-probe log + stock-contract reasoning, not capture. frameRateLimit temp
  key REMOVED from stage-B cfg (done).

## Live round 2026-08-14 (second field session)
- [x] c2 gl05 x64 load — **LIVE-VERIFIED by Kenny**: rasterMajor=5 on stage-B booted all the
      way to Tatooine, zero FATALs. Bonus: the mahjong getTable probes logged the redesigned
      c1 WARN+NULL behavior on the D3D9 path. Caveat noted: his gl05 D3DX chokes on ps_1_x
      shaders + our D3D11-oriented stage-B-override .psh files (2d_blur/2d_bloom
      materialSpecularPower) — his gl05 is not a clean visual reference with the override on.
- [x] c16 c2983980e (amended from 33058cbf1) SwgCuiOptUi ten late-NGE widgets optional —
      **field-found**: clicking Options in space FATALed (STRICT_DATA_FATAL
      CuiMediator.cpp:1517 on checkShowToolbarCommandCooldownTimer, /HudSpace.OptMain).
      ⚠ ATTRIBUTION (Kenny corrected): the loaded data = the v3.0 TRE set SAIS gave us —
      the set his client is distributed with (both clients point at it: "D:/Code/SWGSource
      Client v3.0"). So this is a SHIPPING-DATA crash — any of his users clicking Options
      dies. strictData=false would AV instead (lenient NULL, unchecked deref on the next
      line). Ported our battle-tested 5fce7bb83 shape:
      optional flag + pointer reset + null-checked registration, 10 sites. OptUi was the ONE
      file in his tree with zero optional flags (every overlapping file's counts match).
      Built x64 DX11 clean (0 errors/unresolved), exe hand-restaged to stage-B, pushed.
- [x] c16 **LIVE-VERIFIED by Kenny 2026-08-14**: Options opened in space without crashing
      (same click that FATALed pre-fix).
- [x] nebula recheck — RESOLVED 2026-08-15 via capture-pair diff (Capture210 both stage dirs,
      settings matched all four toggles), and the defect is OURS, not his corpus:
      **gl11 space stars render as 1-pixel points** (Direct3d11.cpp:326-345 no-ops ALL
      point-sprite/point-size state, self-documented deferred Plan 11-11 debt, names
      StarAppearance::draw) while HIS port GS-expands sized point sprites (SwgPointSprite GS,
      5,461 sprites vs our 3,990 raw points). Measured: 1,218 bright star px (his) vs 150
      (ours) — 8x; a dense star cluster reads as a "white nebula" in his, near-invisible in
      ours. HIS client matches DX9 reference here. Killed along the way: textures identical
      both sides (512x512 stock); NO bloom composite ran in either capture (his final draw =
      passthrough blit). His corpus e_nebula stays exonerated.
      ⚠ DATA LANDMINE documented: ILM_visuals.tre carries a 240-BYTE STUB of
      texture/nebula2_front.dds (Legends preference-kill, same class as interior fog) — his
      cfg mounts raw ILM_visuals so our stage-B-override loose nebula2 set (byte-identical to
      data_sku1_03 stock) is LOAD-BEARING on his client; ours is immune (ilm_extract, no raw
      ILM tres). If his stock corpus ships ILM_visuals, flag the stub set to Sais.
      → OUR-SIDE FIX QUEUED: gl11 point-sprite emulation (GS expansion; his impl = reference;
      B→A adoption candidate).

## Wrap-up — DONE 2026-08-14 night
- [x] Branch pushed (tip now a656d9191 post-rebase); PR #1 retitled + full body (corpus gap, bloom-was-stock,
      runtime verify list for Sais: gl05 x64 loads / startup focus / drain log line).
- [x] PR #1 CLOSED 2026-08-14 per Kenny — no review pressure on Sais; branch keeps growing
      quietly. REOPEN (gh pr reopen 1) or open fresh when Kenny says the set is ready.

## Live round 2026-08-15 (third field session — star fix + nebula-cluster conviction)
- [x] gl05 reference side-by-side (both clients rasterMajor=5): **stock has the DENSE stars**
      — his gl11 is faithful, our gl11 was the deviation. Our client's gl05 boot also banked
      the Gl_api guard first exercise (silent pass) and the JTL packed-map test PASSED
      (component names sane in the ship UI).
- [x] OUR star fix LANDED + LIVE-VERIFIED: Direct3d11_PointSprite (new .h/.cpp in our gl11) —
      GS point-sprite expansion adapted from HIS Direct3d11_PointSprite design (rebuilt for
      our per-shader signatures + applyPreDrawState chokepoint; GS in/out = the stars VS
      signature {SV_Position, COLOR0, FOG}); kill switch [Direct3d11] pointSprites=false.
      Kenny: "stars look right now". UNCOMMITTED as of the verify.
- [x] "Ours clearer/brighter in spots" RESOLVED (Capture211 pair): the bright cloud above the
      reticle = a **ClientNebula quad cluster** (shader/pt_nebulae_gas_4_2.sht →
      effect/e_nebula_emisadd.eft + texture/pt_nebulae_gas_4_2.dds). Data equal (table in
      patch_11_00 + ILM + TOC), per-machine options exonerated BY BYTE-PARSE
      (globalNebulaRange 16384 both; density ours 60 / his 120 — his HIGHER), engine files
      byte-identical (ClientNebula, NebulaManagerClient modulo our lightning null-guard).
      **HIS client draws ZERO nebula clusters against stock data**: the effect's stock
      programs are pre-SM4; his framework compiles the VS (a_vertexlit.vsh, 51 log hits) but
      NEVER attempts pixel_program/e_nebula_emisadd.psh (zero log lines; framework logs every
      compile) → silent implementation rejection → all pt_nebulae draws dropped, every space
      zone. OURS renders via our stage/override ported e_nebula_emisadd.psh (602 B; differs
      from the corpus copy in stage-B-override) + Phase-19 fallback lanes. His own
      _client_dx11 dataset presumably papers over this with his converted corpus — which is
      NOT in the squash repo (the PR-body corpus gap, now with its first concrete symptom).
      → FLAG TO SAIS with this repro; his-side fix = ship/deploy the corpus conversions or
      add a fallback lane; the silent-skip (no log on implementation rejection) is itself
      worth a WARNING in his framework.

## Nebula-cluster arc CLOSED 2026-08-15 (his branch now 19 commits, pushed)
- [x] c17 33ffc4d51 phantom input elements (InputLayoutCache+Draw): reflect-and-retry failed
      layouts, phantom stream slot 15, COLOR reads WHITE / others zeros (D3D9 per-usage
      defaults) — THE fix that restored his in-zone nebulas; LIVE-VERIFIED (clouds render,
      "space looks correct")
- [x] c18 495913c0f data-shader failures warn-and-skip (ShaderCompiler dev FATAL +
      ShaderReflection mid-row blanket FATAL) — stock-faithful; crash-repro'd both ways live
- [x] c19 c80a23e03 D3D11_VERTEX_SHADER_CONSTANTS marker in his served include — enables
      portable shader sources (first consumer: our tfcl family, X4019 alias fix)
- [x] temp probes ([neb.probe] in NebulaManagerClient/ClientNebula) REVERTED before commit
- stage-B-override is now a FULL sync of our 4 shader override dirs + the 3 ILM-landmine
  loose fixes (stock nebula table + 2 gas textures); backup of pre-sync state at
  stage-B-override-pre-oursync2-bak
- OUR side landed same day: gl11 GS point sprites (Direct3d11_PointSprite, adapted from HIS
  design; [Direct3d11] pointSprites kill switch) + portable tfcl guards
- QUEUED our-side follow-up: mirror COLOR-reads-white into OUR phantom zero buffer
  (Plan 11-09.8 getPhantomZeroBuffer is all-zeros — same latent invisible-draw hazard)

## Field round 2026-08-15 afternoon (cantina recheck — branch now 21 commits, pushed; c20/c21 SHAs post message-reword force-push)
- [x] c20 52ba6fc36 the two inherited lighting patches become opt-in config — Kenny's live
      report "people in the cantina look a little bright" traced to Direct3d11_ShaderSource
      transform 3: engine-side `max(lightData.ambient.ambientColor + diffuseSpecular.diffuse,
      0.85)` floor patched into every stock //hlsl program at compile (targets the dot3/skinned
      character family — his header's own words: outdoor characters rendered dark on DX9-x64's
      starved data; on good data it holds characters at 85% minimum light). Transform 2
      (c_ambient mov→add, currently inert on stage-B because the loose corpus copy wins string
      resolution) gated too. New knobs: `[Direct3d11] ambientBoost` (bool, default false; the
      re-enabled add is now COLOR-only with alpha PINNED per the 1.85-alpha hazard note) +
      `diffuseFloorPercent` (int, default 0 = stock; 85 = his old look). Both WARN when set.
      Cache-safe by construction (compile key hashes post-patch text). Built x64 DX11 clean,
      gl11_r.dll hand-restaged to stage-B-x64 (exe unchanged — plugin-only). ⚠ VERIFY PENDING:
      Kenny relaunch stage-B → cantina people should match ours; `diffuseFloorPercent=85`
      restores his old look for A/B. ⚠ FLAG TO SAIS: on HIS datasets (if they still starve
      vertex streams) default-off may resurface dark characters — the knob is the escape hatch;
      candidate corpus-conversation item.
- [x] c21 bf4aef663 synthesized hemisphere becomes opt-in — Kenny's post-c20 recheck ("much
      better... still a touch brighter, like a light shadow layer is missing") CONVICTED BY
      CAPTURE PAIR (Capture220, ours stage/ vs his stage-B-x64): world geometry at parity
      (floor 0.95x, wall 1.02x — c20 pixel-verified) but characters ~1.9x (robe region 52 vs
      100/255). Same robe fragment (depth 0.939 both, prim-matched), same interpolated vertex
      lighting v1=0.24 both → the delta is in PS dot3 constants. Mechanism: his
      setExtendedLightData synthesizes tangent=0.65/back=0.30 of key-light diffuse for lights
      authored WITHOUT hemispheres (inherited DX9-x64 fix #4) → +65% key light on every
      character unconditionally, shade side never falls — exactly "shadows look missing".
      Stock ref 0.19, his 0.41. Gate: `[Direct3d11] synthesizeHemisphericLight` (bool, default
      false), WARN when set; LightManager.h inherited-fixes list updated (kept #1/#3/#5,
      removed #2, knobbed #4). Built x64 DX11 clean, gl11_r.dll restaged to stage-B.
      c21 VERIFIED by Kenny 2026-08-15 ("looks really close, close enough for now") —
      formal side-by-side render test deferred to POST-MERGE (Kenny's call). NOTE:
      message-reword force-push (apostrophe fix) changed c20/c21 SHAs; content identical.
- [x] Cantina FOG question dispositioned (Kenny asked "still rendering fog after TRE syncing"):
      fog rendering IS the correct, deliberate outcome — retail's smokey haze; the ILM/Legends
      landmine turned it OFF (interior.iff 'Fog Enabled' 1→0, ~124 interiors); our merged table
      (8cd8c2d82) in stage-B-override at priority 12 restores it and was live-verified 08-14.
      Do not re-report as residual. If NO-fog is ever preferred, that's a preference decision
      for the post-merge TRE cleanup manifest (keep/restore/knob), not a defect.
- [x] tangentColor "flat add" suspicion (parked 08-14, 4 diffuse*.inc sites) REFUTED by
      derivation: `r7 = tangentColor + r7` is the BASE TERM of the stock hemispheric ramp
      (dot=1→diffuse, 0→tangent, −1→back; constants from Direct3d9_LightManager
      setExtendedLightData). Not a compensation — do NOT strip it.
- [x] OUR side: phantom COLOR-reads-white mirror landed (9283264c9, local) — 32-byte phantom
      buffer zeros+white, COLOR elements at offset 16 (mirror of his c17); all four gl11
      configs built + staged both platforms. Retires the queued our-side follow-up.

## Post-merge TRE cleanup (scope sharpened by the 2026-08-15 nebula arc — feeds the parked "ilm sweep" backlog item)
- ILM preference-kill audit: full same-path content-diff of each ILM_*.tre vs the base/patch
  chain (tools/tre-compare is purpose-built). Known landmine classes: 240-byte texture stubs
  (nebula2_* skybox set, pt_nebulae_gas_* cloud sprites), density-ZEROED datatables
  (space/nebula/space_tatooine.iff confirmed; every zone's ILM copy suspect), interior-fog
  preference (known since the JTL arc). Per-entry decision: keep / restore stock / knob.
- Retire the stopgaps: stage-B-override loose fixes (nebula table + 2 textures) and our
  ilm_extract curation are point fixes a cleaned data layer supersedes.
- His distribution mount decision: drop raw ILM mounts (our model) or ship the cleaned layer.
- Data-hygiene catalog (low): stock authoring warts, e.g. e_planet_tatooine.vsh's broken
  DECLARE_textureCoordinateSets backslash — sets 1/2 are DEAD stray globals, shader derives
  all 3 UV sets from set 0 and renders correctly on both pipelines; his reflection WARNING
  is the tombstone. ⚠ Do NOT "fix" the backslashes: that creates real VS inputs the planet
  VB doesn't supply.

## Parked / not in this PR
- dPVS portal fixes (6): downstream of his cellLoaded parenting flip (conflict #3) — needs the conversation
- c_ambient parameterize (ambientBoost, color-only PIN ALPHA): lives in his CORPUS (client-tools), not here
- Char-select wearables retry (async race, no retry on char-select path): his tree, separate investigation
- Screenshot key dead: needs live debug with his symbols
- WorldSnapshot wrong-class narrowing (fetch/instantiate guards from b47718cbc): lower exposure
  in his tree (no editor injecting rows); candidate follow-up, kept out of c13 for scope

## Our repo (swg-client-v2) parallel state
- PUSHED 2026-08-15 (e34760a0a..bdb21dd77) after both gates green (x64 + Win32 serial 5-target,
  0 unresolved externals both)
- was: 7 local commits on master unpushed (adoptions: TextureFormatInfo, write-mask R↔B, packed-map
  int32, transform throttle 60Hz, Archive guards, Gl_dll.def Production.h, Gl_api load guard)
- NEEDS: 5-target build gate BOTH platforms (Archive.h + Gl_dll.def are wide; def can change
  Gl_api layout → all plugins rebuild together), then push
- cfg restored from .swgsource-bak (done, session start)
