# Sais single-PR queue (branch strict-data-defaults, PR #1 DRAFT)

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

## Wrap-up — DONE 2026-08-14 night
- [x] Branch pushed (tip now a656d9191 post-rebase); PR #1 retitled + full body (corpus gap, bloom-was-stock,
      runtime verify list for Sais: gl05 x64 loads / startup focus / drain log line).
- [x] PR #1 CLOSED 2026-08-14 per Kenny — no review pressure on Sais; branch keeps growing
      quietly. REOPEN (gh pr reopen 1) or open fresh when Kenny says the set is ready.

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
