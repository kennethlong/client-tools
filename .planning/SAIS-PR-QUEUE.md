# Sais single-PR queue (branch strict-data-defaults — **THE PR IS OPEN as GitHub #2**, 2026-08-16)

**⚠ NUMBERING CHANGE (2026-08-16 late): Kenny called the reopen. GitHub refused to reopen
PR #1 (head branch was force-pushed after closing — the c20/c21 message reword — permanent
refusal), so the deliverable PR is now GitHub #2**: same branch, same current body, 54
commits, OPEN; old #1 carries a superseded-by comment. **The planned "PR #2" (toolkit
advertise surface) therefore lands as GitHub #3** — planning docs keep the name "PR #2
(advertise)"; map accordingly. COVERT MODE ENDED — the branch is live for Sais's review.

**Restore-knobs ledger: [SAIS-KNOBS.md](SAIS-KNOBS.md)** — every hard-coded behavior of his
the branch removed/gated + the exact cfg key or file copy that puts it back.
**PR #1 body REFRESHED 2026-08-16 LATE (post-c54, "54 commits") — now LEADS with the
stock-acceptance claim ("It runs the stock dataset, as shipped" — the joint-upstream bar),
adds the "Stock-data self-sufficiency" section (c53 embedded corpus + c54 c_ambient
root-cause with the compensation-removed framing), annotates the ambientBoost ledger row as
currently INOPERATIVE (include patch greps asm text; embedded texts inline includes —
reimplementation-as-macro question flagged), and rewrites the no-corpus Known Gap as
resolved (remaining: loose-corpus-in-client-assets decision + silent-skip WARNING).**
Previous refresh (post-c42): added four
sections — load smoothness/perf ports (9c), audio dropout arc measured-and-closed (8c),
Win11 screenshots F12+WIC (2c), the six dPVS portal fixes (1c) — plus a direct
cellLoaded-flip conversation ask in Known Gaps (rationale contradicted in his own tree;
double-ownership hazard; offer to find his original symptom's real cause together) and the
dpvs x64 tidyings note. Verified section carries the field numbers. PR stays CLOSED.**
Previous refresh 2026-08-15 (post-c21): covers all 21 commits, knobs ledger folded in
as a restore table, flag-to-Sais items included (corpus-gap nebula symptom + silent-skip
WARNING suggestion, ILM landmine audit proposal, D3D9-is-ours disposition). ⚠ DO NOT REOPEN
at milestone points — Kenny 2026-08-15: the whole deliverable presents as ONE GIANT PR; the
branch keeps growing quietly and Kenny explicitly calls the reopen. Keep the closed PR's
body current as commits land.**

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

## Session 2026-08-15 evening (one-giant-PR mandate confirmed; branch now 22)
- [x] Kenny 2026-08-15: NO milestone reopens — ONE GIANT PR at the end; branch grows quietly,
      body kept current, Kenny calls the reopen.
- [x] c22 5f68728be WorldSnapshot wrong-class narrowing (the parked candidate follow-up from
      c13 scope): asSharedObjectTemplate at fetchObjectTemplate + asClientObject on the
      created object in instantiateObject (Release safe_cast = bare static_cast; base
      createObject returns plain Object). Adapted from our b47718cbc 6.1 hunks; the
      engine_ws* shim hunks not ported (toolkit advertise surface absent from his tree);
      result out-param left unset on refusal = his tree's existing fetch-failure pattern
      (caller switch has tolerant default). Built x64 Release DX11 clean (0 unresolved,
      exit 0), exe + gl11_r.dll hand-restaged to stage-B-x64 (hashes match build output),
      pushed. PR body updated to 22 commits with the narrowing bullet.

## PR #2 (advertise) — ✅ SHIPPED 2026-08-16 late as **GitHub PR #3**, stacked on #2

Branch `toolkit-advertise` (off strict-data-defaults tip), OPEN, 2 commits:
- `da91bbaa1` Gl_api v35 tail slots (frame/resize overlay callbacks; HIS gl11 implements
  natively in Direct3d11_SwapChain present()/resize(); D3D9 ignore rows restored; Graphics
  guard-and-WARN forwards; **sizeof(Gl_api) changed → matched 6-binary set only**).
- `67b109785` the advertise surface: engine_* files + 24 shim-bearing files hunk-ported +
  support chains (ExitChain shutdown-phase, Game tick callback, WorldSnapshot shim family +
  WSRW saveFiltered/intern, TreeFile::enumerateFiles, interior-refresh chain, Os WM_SIZE) +
  vcxproj (+sharedCommandParser/sharedCollision include dirs) + tools/hookpoints-probe.
- Gates: full x64 Release All build CLEAN (0 unresolved); **probe PASS version=35 count=165
  nulls=0 uniqueNames=165**. Matched set deployed to stage-B-x64. **Boot smoke PASSED (Kenny 2026-08-16 night): DX11 AND D3D9 to ground, no issues** — PR #3 checklist closed.
- Port discipline: WorldSnapshot merged insert-only (his POB-CRC-proceed KEPT); out-of-scope
  candidates for later fix-queue commits: Os.cpp x64 keyCode-extraction bug (REAL Release
  input bug) + menu/ShellExecute pointer-width, ClientMain timeBeginPeriod(1) + bootTrace,
  DebugHelp minidump-reserve, CreatureObject naked-NPC wearables retry, ClientWorld
  logCellAtPosition probe.
- Toolkit coordination still owed: their injector x64 + DetourXS-on-x64 (their side).

## PR #2 original plan (historical): the toolkit advertise surface
Kenny: "We will be adding the whole toolkit advertising code as a second PR." Separate from
the fix-queue giant PR #1; not started. Scoping facts banked now:
- Surface inventory (our tree): `engine_advertise.cpp` + `engine_hookpoints.h/.inc`
  (contract v33 / 160 names; SWG-Toolkit is the SOLE consumer) + 6 `engine_*_forward.h`
  headers, all under `SwgClient/src/win32|shared`; plus shims embedded in clientGame:
  WorldSnapshot.cpp (19 engine_ shims), CreatureObject.cpp, ClientInteriorLayoutManager.cpp,
  PlayerCreatureController.cpp, GroundScene.cpp, ClientEffectManager.cpp, Game.h, Os.cpp,
  ClientMain.cpp/.h, SwgClient.vcxproj (ord-82 export).
- ✅ Design question RESOLVED (Kenny 2026-08-15: "we will do the x64 port") and the ENGINE-SIDE
  PORT IS **COMMITTED**: `372e7aa42` (port, 21 files) + `e292a3478` (tools/hookpoints-probe
  boot-free contract gate) + `971805d5c` (v34 wave: +advertisedArchBits +setScale answering the
  toolkit's x64 CHANGE-REQUEST; contract now v34/162 both arches; handback mirrored to their
  repo). Local commits, push pending. Detail:
  * All `#if !defined(_WIN64)` guards removed across the surface (engine_advertise.cpp,
    GroundScene.cpp/.h friends, Os.cpp/.h friend, DebugHelp.cpp, SwgCuiChatWindow.cpp/.h,
    ClientEffectManager.cpp, CreatureObject.cpp, PlayerCreatureController.cpp,
    ClientInteriorLayoutManager.cpp, WorldSnapshot.cpp x3, ClientMain.cpp verify gate,
    vcxproj Platform=Win32 ClCompile condition).
  * THE real ABI item: the Win32 `__fastcall(pThis, dummy EDX, ...)` __thiscall emulation is
    WRONG on x64 (single convention; dummy shifts args one register right). New `ENGINE_THIS`
    macro seam (engine_advertise.cpp + engine_groundScene_forward.h + GroundScene.cpp
    mirrored block; GroundScene.h friends carry explicit per-platform signatures). Consumer
    `__thiscall` typedefs work unchanged on x64 (keyword inert).
  * pmfToVoid/pmfRealEntry port as-is: x64 SI-PMF is pointer-sized; MI-PMF keeps pfn@0 +
    int adjustor@8, MiPmf reads both; delta==0 held for every real-entry row (probe below).
  * EngineWsNodeInfo frozen 80-byte layout is IDENTICAL on x64 (static_asserts kept as proof).
  * Gates: x64 + Win32 Release 5-target both 0 unresolved/0 errors. dumpbin: x64 exports
    GetEngineHookPoints (ord 82, same as Win32 by shared export-set ordering; binding is
    by NAME); Win32 ord-82 @ 0x00701420 UNCHANGED (zero drift).
  * LIVE consumer-shaped probe BOTH exes (LoadLibraryEx DONT_RESOLVE + GetProcAddress + call
    — the pre-CRT path the static-init race fix engineered): version=33 count=160 nulls=0
    dups=0 on x64 AND Win32. Contract untouched v33/160, no consumer re-sync owed.
  * Boot smoke pending Kenny (agent-shell boots invalid); Debug boot additionally runs
    engine_verifyNoNullNoDup.
  * Remaining PR #2 scope: port the surface files into HIS tree (incl. the c22-skipped
    engine_ws hunks) + SWG-Toolkit x64 consumer work (their injector + DetourXS-on-x64 for
    the DETOURED rows — their side, coordinate).
- c22 note resolved by this plan: the two engine_ws* hunks from our b47718cbc (wsAddObject
  wrong-class REFUSE-before-id-mint + wsForgetNode intern-decision comment) ride along with
  the surface port — do not forget them when PR #2 assembles.
- ⚠ ALSO RIDES WITH PR #2 (added 2026-08-16, D3D9 wholesale wave `079eb8a22`): the two v35
  Gl_api TAIL SLOTS (`setFrameCallback`/`setResizeCallback`, the no-detour overlay rows).
  The D3D9 port deliberately STRIPPED the accept-and-ignore rows from Direct3d9.cpp because
  his Gl_dll.def lacks the slots (kept the wave renderer-scoped; sizeof(Gl_api) load gate
  preserved). PR #2 restores them as a MATCHED SET: his src/win32/Gl_dll.def tail rows +
  Graphics.cpp/h plumbing + gl11 present-path/resize invocation + re-adding the D3D9
  ignore rows (copy from our Direct3d9.cpp ~978-990 + the two ms_glApi assignments).
  sizeof(Gl_api) changes ⇒ FULL plugin-rebuild cascade in his tree (all plugins + exe,
  one staged set — never mix with pre-PR#2 DLLs).
- Toolkit coordination: contract bump/re-sync rules apply (mirror handoffs to
  D:/Code/SWG-Toolkit/.planning/handoff/); any divergence between his tree's shim behavior
  and ours becomes a consumer-visible contract question.

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

## ⚠ STOCK-DATASET ACCEPTANCE (2026-08-15, Kenny) — the reviewer's environment

Reviewers will evaluate the PR against the **stock SWGSource Client v3.0 dataset**
(`D:/Code/SWGSource Client v3.0/` — "they grab it from their shelf"). That install is a
complete, runnable, PRISTINE 32-bit stock client: `SwgClient_r.exe` + gl05/06/07 (no gl11),
stock `dpvs.dll`, stock Miles, `options.cfg rasterMajor=5`, and `login.cfg` already pointed at
192.168.1.200:44453 with `allowMultipleInstances=true`. It is ALSO the dataset both our
clients read game data from, so it is the true common baseline. Do not edit its cfgs
(header: "This is a tracked file. Please dont edit it.").

**Acceptance bar: the branch must work against that dataset with NO curated data layer** —
no `stage/ilm_extract`, no `stage/override`, no `stage-B-override`.

### BLOCKER FOUND — his DX11 needs an offline shader corpus that is NOT in the deliverable repo

- `Direct3d11_ShaderCompiler.cpp:450` + `refuseIfNotHlsl()`: his DX11 **hard-refuses D3D9
  shader assembly** — "assembly program cannot be built at all; the asset pipeline translates
  the reachable ones" / "It needs translating to HLSL." Stock SWG shaders ARE D3D9 assembly.
- His repo ships **ZERO** `.psh/.vsh/.hlsl/.fx` files. The translated corpus lives in
  `D:/Code/Galaxies-Reborn/stage-B-override` (**131 pixel + 113 vertex = 244 programs**,
  mounted at priority 12) and the asm2hlsl generator lives on `client-tools@x64-dx11-vanilla`
  — neither is in `swg-source-x64-dx11`.
- ⇒ A reviewer building his branch against the shelf dataset gets a DX11 client that refuses
  a large fraction of shaders. **DX11 is not self-sufficient in the PR as it stands.**
- CONTRAST — ours degrades instead of refusing: gl11 has a generic `//asm` fallback VS +
  skip-draw paths (`Direct3d11_VertexShaderData.cpp:159`, `Direct3d11_StateCache.cpp:1379`,
  `Direct3d11_VertexDeclarationMap.cpp:150`), and `stage/override` is only **11 pixel + 9
  vertex** reauthored `//hlsl` programs (106 files total).
- **FLAG TO SAIS + decide:** ship the corpus as bytes, ship the generator (recipe-not-bytes,
  matching the upstream-offering stance on ilm_extract), or adopt our asm-fallback so stock
  data renders unaided. Untested either way — needs a stock-cfg run of his client.
- NEXT CONCRETE STEP: build a stock-parity cfg for stage-B (mirror the v3.0
  `client.cfg`/`live.cfg` layering: `swgsource_3.0.tre` + the 4 TOCs + `disable_wayfar_dearic_snow.tre`,
  ILM commented OUT, no override corpus) and see what his DX11 actually does.

### ✅ RESOLVED 2026-08-16 late — c53 `5232bb632`: the corpus is now IN the DLL

- The stock-cfg A/B run happened (byte-identical stock cfgs in stage-x64 + stage-B-x64; live
  cfgs backed up as `client-live.cfg.bak-stockrun` — **restore both after the arc**). B black
  login confirmed the blocker exactly as predicted.
- Ground truth by scan (not sampling): stock mount = **806 programs, 224 asm; corpus covers
  221/224** (misses = membrane + water_pass2_20/25, ps.1.x with ps20 HLSL siblings that win
  validation — Theed water verified). Stock `.vsh` = plain text, `.psh` = IFF/PSRC.
- Resolution = third option, upgraded: `Direct3d11_EmbeddedShaderCorpus` (generated, ~1.6MB)
  embeds all 221 translations with corpus includes INLINED (stock TREs carry ASM files at the
  SAME `vertex_program/modules/*.inc` paths; TreeFile-first include order poisoned un-inlined
  substitutes with X3000 'm4x4'). Pixel hook after `refuseIfNotHlsl`; vertex hook in
  `parseHeader` (re-parses embedded header — tag block drives TEXCOORDs). Mounted corpus
  still wins by contract, so his client-assets distribution path is untouched.
- **Stock DX11 x64 Release VERIFIED: login, char select, cantina interior walls/floor, NPCs,
  Theed water.** Remaining: gl05 stock pass (predicted green — D3DAssemble export handles
  //asm; stock pixel PEXE is real D3D9 bytecode), then restore live cfgs. PR #1 body owes
  c52+c53 sections at reopen.

### ✅ c54 `8a9021322` — the "tacky bright" interior wash was the CORPUS, not ILM

- Kenny field-caught DX11-bright vs DX9-moody on the SAME stock data, both clients →
  renderer-class. RenderDoc Capture240 (cantina ceiling pixel → draw 4345) one-trace
  conviction: baked vColor0 correct (0.28 grey), corpus c_ambient.inc translation adds scene
  ambient (1,1,1) → saturate → raw albedo. Stock asm c_ambient = bare `mov r7, vColor0` —
  the bake IS the cell lighting. CONSULT-74 had already corrected this in stage-B-override;
  the embedded table was generated from the uncorrected `_client_dx11` drop. c54 regenerates
  with stock semantics. Verified: cantina matches DX9; Theed water + space/nebulas good.
- **FLAG TO SAIS (two items, fold into PR body/conversation):** (1) the distributed corpus
  still ships the boosted c_ambient.inc → a mounted-corpus client washes every interior the
  same way (likely the origin of "that's just ILM lighting"); (2) the [Direct3d11]
  ambientBoost include patch greps the ASM text `mov r7, vColor0`, which can never match the
  corpus's HLSL module — the opt-in gate is dead against the current corpus.
- **A-SIDE FOLLOW-UP (our repo):** our gl11 shows the same wash on stock (the "black walls
  full ambient" fix is the same compensation class) — adopt bake-is-lighting, RenderDoc
  re-verify, then re-baseline the ILM "brightening" claims (see ILM audit section).

## Perf + smoothness port A -> B (2026-08-15, Kenny: "take any relevant improvements from A into B")

Scope: perf AND movement smoothness into the ONE giant PR; advertise surface stays PR #2.

LANDED on `strict-data-defaults` (pushed, branch now 27):
- `3937fe234` sharedCollision floor-seam shallow-graze gate — the **floor half only** of our
  door-snap fix (`cs_seamGrazeEpsilon` 0.05m). Kenny verified: sideways snap FIXED.
- `352a68488` sharedFile async-loader callback drain wall-time budget
- `c23498847` clientGame interior-layout creates spread across frames
- `261522e5f` clientGame world-snapshot create/delete drain wall-time budget
- `620ce5d2b` clientGame stall watchdog + audio-safe stack sampler (incl. the symbol pre-warm)

⛔ CAMERA — **NOTHING from A goes in** (Kenny reversed twice; final position):
- His snap-back-when-it-cannot-ray-trace is CORRECT/desired. Our smooth compensation is
  exactly what lets our camera drift outside walls. So: no pull-in rate limit
  (`cs_cameraPullInSpeed`), no convergence follow-up, no interior zoom cap.
- Interior zoom cap was ported then REVERTED (Kenny: "the original game didnt have that
  camera cap, and it feels bad in game play mode"). Also DISABLED in A via
  `stage-x64/client.cfg freeChaseCameraInteriorMaximumZoom=0`.
- The cap was only ever a MITIGATION for the interior portal cull -> the real fix is dPVS.
- OPEN: his camera oscillates in/out against wall-mounted terminals in the cantina foyer.
  Kenny: check STOCK client first — if stock oscillates too it is 20-year-old retail
  behaviour and we do not spend time on it.

STILL TO PORT (frame-spreading family):
- ~~phased WorldSnapshot load~~ **PORTED `c004ba687` + VERIFIED same day (Kenny: "buttery
  smooth" — the same phrase as our July verify; log clean, drain coexisting, zero create
  failures)**
  — the feared hand-port turned out clean: his load() was our pre-CONSULT-60 baseline
  nearly line-for-line (his file is smaller only because it lacks our editor surface).
  Full mechanism ported: incremental ReaderWriter, prologue+loadStep state machine
  (wsNodes → buildout-per-area → sphereTree 4096/batch), 11 exactness valves, unload
  cancel, parse-aware donePreloading/getLoadingPercent, updateLoading pump, preload
  callback slice 1s→50ms (CONSULT-61). `[ClientGame] worldSnapshotParseBudgetMs` (40;
  ≤0 = old sync path = kill switch). Verify: zone-in smooth, no music hitch, world
  complete on entry.
- ~~budgeted terrain preload~~ (ported 0da2d4b7c) · ~~TreeFileFactory buffering + Texture
  one-read~~ (ported 78c9697fe/4856bcf4f)
- ~~searchPath negative cache + file manifest~~ **PORTED `7e9c653d8` 2026-08-16 (⚠ awaiting
  verify — passive: [treefile.probe] lines at clean exit show realProbes; A/B recipe in the
  commit)**. Full mechanism: negCache + manifest + forgetMissingFile broadcast + probe
  counters + prio-100 ExitChain report. Both keys default true; kill switches
  `[SharedFile] searchPathNegativeCache/searchPathFileManifest=false`. B's TRE-0006 TOC
  support preserved (hunk-port, not copies, except the exactly-matching Config pair).
  Gotcha re-learned: Copy-Item preserves SOURCE mtime → MSBuild skipped recompiling the
  copied Config .cpp → LNK2019; touch copied files before building.
  **THE PERF PORT QUEUE IS NOW EMPTY.**
- ~~dPVS portal fixes~~ (ported eb1b26024, soak watch)

~~CHEAP WIN: shader manifest bake~~ **DONE 2026-08-15 (198 blobs, cache live every session:
"cache enabled — 198 baked, 17 includes verified", ~185 hits/session). Residual found
2026-08-16: 4-7 misses EVERY session (gradient_sky.vsh/.psh, cloudlayer.vsh/.psh,
sometimes a_scroll_rgb1_a2/a_splitalpha — ~30-48ms + one ~28ms hitch frame) — the original
bake session never compiled the sky/cloud set, and his baker REPLACES the manifest with
that session's compiles (a re-bake could silently shrink coverage). FIXED `1dbd380f4`:
bake now SEEDS from the existing manifest (same validation as a using run: version, flags,
include hashes vs live; fail-any = bake from scratch) → writeManifest emits the UNION,
includes merged (served ∪ carried-validated). `dxbcbake` (referenced in his comments) does
NOT exist anywhere — in-game bake is the only mechanism.
**INCREMENTAL RE-BAKE VERIFIED 2026-08-16: seed line fired ("baker seeded with 198
program(s) and 17 include hash(es)"), exit wrote "baked 204" — the 6 additions are EXACTLY
the six known offenders (keys match the miss lines byte-for-byte). Cfg flipped back to
`bakeCompiledShaders=false`; manifest backup kept at `compiled_shader/manifest-198-20260815.bak`.
Follow-up `214c98165`: seed parse keeps multi-word program names (the %s parse truncated
"Direct3d11 point sprite expander" → remainder-of-line parse; current manifest line
hand-repaired). Passive verify: next sessions' metrics line should read ~190 hits /
0 misses.**

CFG: his stage cfg's "TUNING KEYS WITHHELD" note claimed all three were "stock keys available
to both trees" — only `minFrameRate` was. Corrected; `asynchronousLoaderCallbackTimeBudgetMs=6`
and `maxInteriorCreatesPerFrame=10` now live, plus `stallWatchdogMs=100`/`MaxDumps=0`.

## ISSUE STACK — login-position regression (REPORTED 2026-08-15 ~21:30, UNINVESTIGATED)

Kenny: exited A from the Mos Eisley cantina, logged back in, and spawned at the
starport/station instead — "newish behaviour." His hypothesis: a client (possibly B) is not
sending something the server needs to persist last location.

⚠ TWO CONFOUNDS to rule out FIRST before blaming any client change:
1. **Multi-client last-writer-wins.** A and B play the SAME character on the same server, and
   both were logged in repeatedly tonight (B went to space ~19:48; A was in the cantina ~20:47
   and ~21:06). The persisted position is whatever the LAST clean disconnect wrote — an
   interleaved B session can legitimately overwrite A's cantina position.
2. **The after-space mechanic.** Returning from a space session historically places the
   character at the launch starport. B's space trip may have armed exactly that.

Evidence available when investigated: A's SwgClient_report.log logout sequences ([shutdown]
phase lines timestamp every clean exit), B's report log ditto, and the server's own logs.
Nothing in today's A changes touches networking (probe emission, sampler pre-warm, VOLSET
seed, shader inventory, log sharing); B's changes are audio/perf — the only network-adjacent
port is the WorldSnapshot create/delete budget (engine-internal, no wire traffic).

## Parked / not in this PR
- dPVS portal fixes (6): downstream of his cellLoaded parenting flip (conflict #3) — needs the conversation.
  **FIELD-EVIDENCED 2026-08-15 22:01** — Kenny captured the classic foyer see-through in B
  (screenshot `C:/Users/kenne/Downloads/Screenshot 2026-08-15 220121.png`, Mos Eisley 3457,5,-4844:
  raw sky/terrain through the portal frame). Second sighting that evening (first was live, no capture).
  **THIRD sighting 2026-08-16 ~10:19, same classic foyer location** (during the audio round-3
  verify run). Reproduces reliably in normal play — the conflict INVESTIGATION is now overdue,
  not just the entry ticket for a nice-to-have.
  **CAPTURE-CONVICTED 2026-08-16 10:30 — SAME ISSUE CLASS AS OUR A-SIDE dPVS BUG.**
  Kenny captured the glitch in the act (`stage-B-x64/Capture230.rdc` + paired F12 screenshot
  `screenshots/screenShot0000.jpg`, same instant). Pixel history on a void pixel through the
  arch (900,250 @ event 652): clear → two skybox layers at depth 1.0 → NOTHING — no interior
  fragment ever submitted, not even a failed one ⇒ the adjacent cell was culled at the
  VISIBILITY layer (dPVS), not depth/state-rejected. Void is exactly portal-frame-shaped;
  frame has only 152 draws. Scene RT export: `renderdoc-mcp-export/rt_652_0.png`. The 6-fix
  port is now evidence-justified; sole remaining gate = the cellLoaded parenting-flip
  conflict investigation.
  **PORT LANDED `eb1b26024` 2026-08-16 (pushed; ⚠ awaiting Kenny foyer verify).** Conflict
  investigation resolved the gate: his flip (`attachToObject_p(..., true)` in
  PortalProperty::cellLoaded, rationale in his comment) is refuted by his OWN tree — his
  Container::addToWorld contents-walk is intact (his Container.cpp delta vs ours =
  formatting only; Object.cpp IDENTICAL), so `false` suffices there exactly as in stock/A.
  Port scope: 5 wholesale file copies (dpvsDatabase / dpvsImpMeshModel /
  dpvsVisibilityQuery_Test / _Traverse — probe counters+exports ride along, self-contained,
  inert — + DoorObject) + FreeChaseCamera hand-merge of the two camera-cell fixes (re-tag
  only, no feel change — does NOT violate the camera mandate; both CellProperty APIs
  verified present in B). **Deliberately NOT reverted: the cellLoaded flip** — culling-layer
  fixes are independent of ownership semantics; one variable at a time keeps the foyer
  verify attributable. Flip revert = separate future commit (the PR hill: double-ownership
  hazard). RenderWorld probe suite NOT ported (B's RenderWorld has his own APIs; port only
  if the repro survives). FOUND ALONG THE WAY, not acted on: B lacks our dpvs x64 cleanups
  (ImpObject Entry 64-byte alignment pad, UPTR guard casts, `register` keyword) — cache/warning
  class, note for the eventual merge. Verify: foyer at 3456,5,-4844, pre-fix ~1-in-7 logins.
  **VERIFY 2026-08-16: 3 clean relogs at the classic spot (Kenny) → SOAK WATCH** (pre-fix
  ~1-in-7; keep watching during other testing; a recurrence gets an F12 shot + capture
  against the fixed build → pixel history names the survivor).
  **cellLoaded flip REVERTED `86606befe` 2026-08-16** — Kenny: "Sais told us to fix it all,
  no conversation needed." Landed after the dPVS verify (attribution clean). Pre-revert
  verification: his handleEndBaselines + Container::addToWorld contents-walk = same
  machinery ours runs `false` through daily; his endBaselines on-demand load + POB-CRC
  proceed KEPT. **VERIFIED 2026-08-16: Kenny city login + cantina interior normal; log
  shows ZERO cell/portal warnings. Same run = shader-cache passive verify PASSED (204
  baked, 191 hits / 0 misses — first zero-compile session) + 4th clean dPVS soak login.**
  PR body updated to 45 commits (flip section added; Known-Gaps ask retired, replaced with
  an if-you-remember-the-original-symptom note).
  **Audio round-4 passive verify SAME RUNS: 0-2 masked dropouts/session (was 22), 55
  recovered waits incl. 2257µs/1100µs recoveries — the yield phase catching exactly the
  preempted-holder class round 3 dropped. Ear clean. Drip arc holds under real play.**
- c_ambient parameterize (ambientBoost, color-only PIN ALPHA): lives in his CORPUS (client-tools), not here
- Char-select wearables retry (async race, no retry on char-select path): his tree, separate investigation
- ~~Screenshot key dead: needs live debug with his symbols~~ **RESOLVED 2026-08-16 by A→B
  diff, no live debug needed** (`4a17bc275`): Windows 11 Snipping Tool's low-level hook eats
  PrintScreen before DirectInput's foreground non-exclusive acquire sees it — the DIK_SYSRQ
  bind can never fire on stock Win11; his render path (Direct3d11_ImageWriter →
  `./screenshots/screenShot%04d.jpg`) was always fine. Ported our F12 backup trigger
  (A has carried it since Phase 11). Kenny: press **F12** in B; files land in
  `stage-B-x64/screenshots/`. **F12 VERIFIED on first outing 2026-08-16 10:30** (paired with
  Capture230) — but the jpg came out stretched 4/3 + period-4 vertical striping + scrambled
  colour. **SECOND writer bug found and FIXED `4f74ae47c`**: WIC `SetPixelFormat` NEGOTIATES
  (JPEG has no 32bpp → comes back 24bppBGR) and `WritePixels` converts NOTHING — his code fed
  32bpp rows to a 24bpp interpretation (proof: measured luminance cycle 146/157/201/173,
  exact period 4 = the 12-byte alignment cycle). Fix = IWICBitmap + IWICFormatConverter +
  WriteSource; TGA path (his debug harness) never used WIC, which is why he never saw it.
  gl11_r.dll restaged (C035C9…). ⚠ PS 5.1 gotcha re-learned: commit messages containing
  double quotes must go via `git commit -F <file>` — inline here-strings mangle at embedded
  `"` in native-arg passing.
- ~~WorldSnapshot wrong-class narrowing~~ — LANDED as c22 5f68728be (2026-08-15, see session
  entry above)

## JUCE audio — drip fix LANDED 2026-08-16 (`604b66bf1`, pushed; awaiting Kenny ear + metric verify)

The designed lock-miss cure (adoption blocker #1 in memory `project_juce_audio_adoption_arc`),
both halves in one commit on `strict-data-defaults`:
- **Hot setters lock-free**: volume/reverb/rate/3D pos+vel+distances/obstruction/occlusion/
  status-poll/master levels/rolloff/listener vectors take NO lock — fields moved to relaxed
  atomics the callback snapshots per block. Sound invariant (documented at `s_mutex` in
  JuceMiles.cpp): every AIL_ entry runs on the game thread = the handle maps' sole mutator,
  so unlocked game-thread FINDs can't race the still-locked insert/erase; `AIL_start_sample`
  still locks, and its unlock publishes all prior relaxed stores (no start-order glitch).
- **Repeat-last-block masking**: a residual try_to_lock miss replays the previous mixed block
  (cursor didn't advance → content-continuous, ~10ms doubled audio instead of a gap), fading
  ×0.6 per consecutive miss. Miles semantics: stale data, never no data.
- Metric intact: `[audio.dropout]` counts every miss (line now says "masked by last-block
  replay"); **lockMisses ≈ 0 + ear sign-off stays the A-clean adoption bar**.
- Build: his `Build-Client.ps1 Release x64 DX11` clean, 0 unresolved externals.
  stage-B-x64 HAND-RESTAGED (exe hash E96880…, gl11 C2539C… both match build output);
  pre-fix log rotated to `audio-decode-predripfix.log` so the verify run starts clean.
- ~~VERIFY (Kenny)~~ **ROUND-1 VERIFIED 2026-08-16 morning: EAR CLEAN (Kenny: no pops) but
  miss rate UNCHANGED** — 48/26,070 (0.184%) vs baseline 65/37,161 (0.175%). The masking did
  the audible work; the setters were never the drip. Physics: miss probability tracks TOTAL
  lock-held time (any overlap = instant try_to_lock failure), so sub-µs setters were
  negligible all along — the ms-class holds are the drip. Log convicts them precisely:
  7-miss burst EXACTLY inside figrin_dan's 316ms fill (2MB copy-in per chunk under lock);
  isolated misses at every stream open (TreeFile read + parse + 1s decode under lock);
  serve's fprintf+fflush under lock.
- **ROUND 2 LANDED `8bcbc9087` (pushed; ⚠ awaiting metric re-verify)**: fill worker decodes
  whole track privately then O(1) buffer-swap under lock (1s first block covers the swap ~9×);
  AIL_open_stream fully unlocked (fresh sample stays SMP_DONE until start → mixSample's
  status check guards it; map mutations still lock); AIL_serve reports before the lock and
  fires EOS callbacks after release (Miles ran them lock-free too). Build clean, restaged
  (exe B188A4…), round-1 log rotated to `audio-decode-dripfix-round1.log`.
- ~~VERIFY round 2~~ **ROUND-2 VERIFIED 2026-08-16: ear smooth again; BURSTS GONE but the
  steady drip held at 0.162%** (22/13,602, evenly spread, no longer correlated with opens or
  fills — the remaining colliders are short-hold traffic). Two-driver theory checked and
  dead (engine opens ONE digital driver).
- **ROUND 3 LANDED `314efe78f` (pushed; ⚠ awaiting verify)**: the callback now spins
  (_mm_pause) up to 1ms for the lock before declaring a dropout — against short holds,
  try-once-and-drop was the wrong policy (lock frees in µs, budget is ~10ms/block). A
  recovered block plays NORMALLY (counted as a wait → new `[audio.lockwait]` line with
  maxWaitUs); a miss surviving the full 1ms now MEANS a genuinely long holder exists.
  Doubles as the discriminating instrument: waits-with-small-maxWait = drip closed by
  construction; persistent misses = keep hunting. Build clean, restaged (exe C2B521…),
  round-2 log rotated to `audio-decode-dripfix-round2.log`.
- ~~VERIFY round 3~~ **ROUND-3 VERIFIED 2026-08-16: ear smooth; dropouts 22→4/16,753 (0.024%,
  all masked); 30 waits recovered ≤84µs — photo-finish theory CONFIRMED.** The 4 survivors
  outlived a 1ms spin = PREEMPTED holder (needs our timeslice; spinning against it is
  anti-productive).
- **ROUND 4 LANDED `2d48b38ae` (pushed; passive verify)**: wait becomes two-phase —
  _mm_pause first 100µs (running holder), `yield` after (preempted holder), budget 3ms of
  the ~10ms block. Expected: dropouts ≈ 0 flat. No dedicated run needed — glance at
  `audio-decode.log` after any future session. **The drip arc is functionally CLOSED**
  (audible dropouts = 0 by masking even in the worst case); remaining JUCE adoption items
  are the hardening list + voice cap, and long-term the lock-free command queue.
- Still open on the arc: hardening list (decoded-sample reuse, float-PCM residency,
  cross-thread createReaderFor), voice cap/priority stealing; long-term = lock-free command
  queue ("adoption done properly").

## Our repo (swg-client-v2) parallel state
- PUSHED 2026-08-15 (e34760a0a..bdb21dd77) after both gates green (x64 + Win32 serial 5-target,
  0 unresolved externals both)
- was: 7 local commits on master unpushed (adoptions: TextureFormatInfo, write-mask R↔B, packed-map
  int32, transform throttle 60Hz, Archive guards, Gl_dll.def Production.h, Gl_api load guard)
- NEEDS: 5-target build gate BOTH platforms (Archive.h + Gl_dll.def are wide; def can change
  Gl_api layout → all plugins rebuild together), then push
- cfg restored from .swgsource-bak (done, session start)

## ILM audit addition (2026-08-16, Debug-boot find): corrupt IFF size fields
- **ALL 102 `.skt` files in `ILM_visuals.tre` are structurally corrupt**: the Legends
  skeleton-LOD strip tool (SLOD → bare LOD-0 SKTM) wrote FORM sizes larger than the payload
  (all_b.skt declares 2954, carries 2938). Debug `IFF_DEBUG_FATAL` catches it at char select;
  Release silently seeks past EOF. Fixed locally: 102 intact stock copies (sku0 chain, full
  SLOD with LODs) written to `stage-B-override/appearance/skeleton/`.
- Audit consequence: the post-merge ILM sweep must IFF-VALIDATE every ILM file (recursive
  size-fit), not just same-path content-diff — the corrupt-header class may extend beyond .skt.
- His-cfg note: `_client_dx11` mounts ILM_visuals; Debug runs there need the same overrides
  (or the ILM unmount decision).
