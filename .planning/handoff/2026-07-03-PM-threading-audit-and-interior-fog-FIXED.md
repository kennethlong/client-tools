# 2026-07-03 PM session — threading-audit backlog CLEARED (CONSULT-57) + interior-fog data regression FIXED

**Status: ALL DONE, committed + pushed. `master` = `8cd8c2d82`.**
Chain: `0547a0a30` (todo retirements + CONSULT-56 evidence docs) → `885b190a0` (threading fixes) →
`8cd8c2d82` (interior-fog data fix). Staged Win32 Release exe rebuilt at HEAD post-session.

---

## Arc 1 — Housekeeping (morning)

- **All 4 staged cfgs rewritten clean** (stage/ + stage-x64/, debug + release; BOM-verified).
  Debug regained: AsynchronousLoader ON (a Phase-19 diagnostic had it DISABLED since then —
  masking async races in Debug!), gameFeatures 33297 (was 15 → no JTL), ilm_extract +
  install-root search paths. Dropped: psrcCensus, post-FX iter relic, Bloom stub override,
  DPVS keys, reportFrameStats, Phase-7 swg_dev_bundle layer, x64 logContainerProcessing.
  Renderer selections preserved per file. cfgs are gitignored — this is local-state cleanup.
- **5 todos retired** (space frontier, space-HUD cfg, JTL test, d3dcompile port, zoomcap-obsolete);
  pending/ now: TRE-asset-diff (informational), SSHT heap-corruption sibling, + 2 new (below).
- Kenny confirmed complete: memory-leak walkthrough, alt-tab device-loss, 3D-SFX arc, zone-in
  optim follow-ups — memories updated. `UTINNI_REQUIRED_COUNT` debrand residual was already done.

## Arc 2 — Threading-audit backlog cleared (`885b190a0`, CONSULT-57)

The CONSULT-56 follow-up list, executed with a 4-AI adversarial review
(evidence/synthesis: `.planning/research/CONSULT-57-*`; verdict grid in the SYNTHESIS):

1. **TreeFile::ms_searchNodes** (vector, runtime-mutated via the advertised `treeFile::searchTree`
   row, read by 4 threads): all 6 unlocked traversals → `copySearchNodes()` stack snapshot under
   `ms_criticalSection` (capacity 256, warn-once + add-side WARNING); node I/O outside the lock.
2. **Texture::m_referenceCount** — the REAL unprotected refcount sibling (the flagged
   ShaderEffect/ShaderImplementation/Video were FALSE ALARMS — already serialized; do NOT "fix").
   Texture fetch/release now under TextureList's recursive CS (the ShaderEffect idiom) — closes
   the counter race AND the by-name resurrection UAF. Layout unchanged → no plugin ABI cascade.
   Also locked the asymmetric bare-++ fetches on ShaderImplementationPass{VertexShader,Program}.
3. **destroyShader WIRED** (dead since the 2003 SOE import): `~ClientChunk` decrements once per
   primitive per ShaderSet (exact per-tile createBlendedShader pairing, verified 3× independently);
   blended-terrain-shader eviction (alter()'s 5s idle) now works for the first time ever. Guards
   fail toward the old leak, never toward evicting a live shader.

**Crew catches that shipped:** Opus found a **deterministic zone-out UAF** in my wiring
(appearance dtor deleted m_shaderCache BEFORE m_chunkTree, whose chunk teardown runs the new
destroyShader) — fixed by reordering; three other reviewers confirmed straight past it. Cursor's
DISSENT (snapshot capacity truncation) + lock-held-FATAL hoists + no-match WARNING; Codex's
install()-reads gap; Sonnet stamped `flushCache` as a dormant twin bug (dead code that clears
without release — warning comment added). Noted-not-acted: TextureList CS now brackets GPU
teardown on release-to-zero (measure only if zone-in hitches appear); pre-existing FileStreamer
unlocked-seek (threaded streamer ON in our cfgs) and TextureList::remove ExitChain bulk delete.

**Gate:** 5-target Release/Win32 ×2 rounds, 0 unresolved. **Smoke: zone-in AND zone-OUT/logout**
(the destroyShader path is teardown — a `destroyShader` WARNING in the report log = accounting
imbalance; a zone-in HANG = lock regression).

## Arc 3 — Cantina "smokey haze" gone → ILM data shadowing (`8cd8c2d82`), NOT our code

Kenny: cantina lost its haze, both renderers; the original SWGSource client shows it. Bisect
exonerated `885b190a0`. Root cause: **interior fog was data-killed on 2026-06-27** when
CONSULT-51's de-stub wired `stage/ilm_extract` (priority 5): ILM_visuals.tre carries SWG
Legends' `datatables/interior/interior.iff` = retail's table with **`Fog Enabled` 1→0 in ~124
interiors** (all cantinas, Jabba's palace, Fort Tusken, Yavin temples, bunkers, newbie hall...)
+ one Kashyyyk ambient swap. Diagnosed by parsing both variants with
`swg-blender-plugin/swg_pipeline/tre_reader.py` + a hand-rolled DTII DataTable parser
(fog chain: `InteriorEnvironmentBlockManager` ← `datatables/interior/interior.iff` →
`CellObject::setFog*` per cell → `Graphics::setFog`).

**FIX:** `stage/override/datatables/interior/interior.iff` (priority 10, tracked) = **merged**
table — 227 retail rows with retail values + ILM's 38 genuinely-new rows (Mustafar/gunboat/
blackwing POBs). Live-verified by Kenny (haze back). **Lesson: ilm_extract's "real updates"
bucket is a Legends-preference vector** — audit todo filed.

## Evening addendum — gl11 stutter triage + a baseline WIN

Kenny reported gl11 frame-rate pressure. A/B kit built + run: **eviction knob
([ClientTerrain] blendedShaderEvictionIdleSeconds=0) ✗, pre-885 binary ✗, x64 ✗** — this week's
threading work AND 32-bit memory pressure exonerated. Last suspect standing: the CONSULT-51 NV
flag, now config-gated (`04c6cee6a`, [Direct3d11] preventDriverInternalThreading; false = perf
A/B ONLY, re-exposes the nvwgf2um zone-in crash). **Test 4 RESULT: CONVICTED** — flag off =
"almost buttery"; the gl11 micro-stutter (1-2 frame hangs) IS the flag's serialization cost.
cfg reverted to flag=true (the crash race must stay defeated for daily use). Follow-up phase
filed: `2026-07-03-gl11-map-discard-churn-reduction.md` — census + reduce Map(WRITE_DISCARD)
churn (dynamic VB/IB ring NO_OVERWRITE strategy, cbuffer batching), then soak with driver
threading re-enabled. Separate residual characterized: first-visit cantina-door pause (~5-6
frames, gone on revisit) = cold-load hitch, NOT the flag, low priority.

**Baseline validation (2026-07-03 evening):** head-to-head on the same machine, same VirtualBox
server load — retail SWGSource client: smooth but DOOR-SNAPS and shows redraw anomalies; **our
gl05: no stutter, no snap, no redraw anomalies — strictly better than retail on retail's own
renderer** (door-snap fix + FPU_PRESERVE + WDDM present/allowTearing work all validated against
the reference). The remaining gap is gl11-only.

## OPEN / next

- **Two follow-up todos filed 2026-07-03** (plans being drafted):
  `2026-07-03-ilm-extract-legends-preference-audit.md` (cutscenes.iff + missiles.iff next
  suspects, then shader/texture/effect overlaps; tools/tre-compare is purpose-built for this) and
  `2026-07-03-audio-pops-3d-sound-delays.md` (check TreeFile-lock contention on the audio IO
  thread before blaming Miles).
- **Carry-over:** maintainer v15 rebind + live smoke (Utinni, consumer-side); zone-in confidence
  accrual on the CONSULT-56 fixes continues; SSHT heap-corruption sibling todo (may be moot
  post-f344/885 — next occurrence writes a real mdmp now).
- Untracked by design: CONSULT-56/57 `.out` transcripts + the CONSULT-57 diff snapshot.
