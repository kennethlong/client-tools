# 2026-08-15 — SESSION CLOSE 2: nebula arc, cross-client shader unification, his branch at 19

**READ FIRST (latest session-close).** Continues the 2026-08-15 close (Sais fix queue landed,
15 commits). This session: the live-verify round completed, the "missing nebula" marathon
root-caused THREE stacked ILM data landmines over a genuine port defect, and the two clients
now run the SAME shader override sources via a portable-source convention. **Live tracker with
per-commit detail: [.planning/SAIS-PR-QUEUE.md](../SAIS-PR-QUEUE.md).**

## Operating mandate (note for future sessions)

**Sais told us to go covert and get it done** ("covert mode, put my OCD to good use") — full
autonomy, no per-fix approvals, we have ADMIN on his repo. The branch commits are written
self-documenting so he can read the whole story from `git log` whenever he chooses. **PR #1
stays CLOSED deliberately** (no review pressure); `gh pr reopen 1` when Kenny says go — and
**refresh the PR body first**: it predates c16-c19.

## State

- **Our repo:** `master` == `origin/master` == `3ce7b4590`, tree clean, pushed.
  New: `64005a05b` gl11 GS point-sprite star fix (Direct3d11_PointSprite; kill switch
  `[Direct3d11] pointSprites=false`; live-verified vs the gl05 reference) ·
  `034684349` portable tfcl.vsh family (stage/override, marker-guarded) · `3ce7b4590` docs.
- **His repo:** `strict-data-defaults` @ `c80a23e03`, **19 commits**, pushed, tree clean.
  c16 `c2983980e` Options optional widgets (LIVE-VERIFIED — the crash was against the v3.0
  set HIS client ships with, i.e. his users hit it) · c17 `33ffc4d51` **phantom input
  elements** (reflect-and-retry failed input layouts; phantom stream slot 15; **COLOR reads
  WHITE, others zeros — D3D9's per-usage defaults**; THE fix that restored his in-zone
  nebulas, live-verified) · c18 `495913c0f` data-shader failures WARN-and-skip (compiler dev
  FATAL + reflection mid-row blanket FATAL both demoted; stock-faithful; both crash classes
  repro'd and cured live) · c19 `c80a23e03` `D3D11_VERTEX_SHADER_CONSTANTS` marker in his
  served include (enables portable shader sources).
- **stage-B-x64:** binaries = CLEAN branch-tip rebuild (probes reverted pre-commit, exe
  rebuilt + hand-restaged; gl11 hash-matched). **stage-B-override = FULL sync of our four
  shader override dirs** (pixel/vertex/texture/appearance) **+ three ILM-landmine loose
  fixes** (stock `datatables/space/nebula/space_tatooine.iff` from patch_11_03; stock
  `pt_nebulae_gas_4_2.dds` + `_alpha_2.dds` from patch_11_01). Pre-sync rollback at
  `stage-B-override-pre-oursync2-bak`. 12 investigation logs (51MB) deleted; RenderDoc
  captures 200-212 kept (Kenny's artifacts).

## The nebula arc (the marathon — full conviction chain in SAIS-PR-QUEUE.md)

Kenny's "he's missing a render" (a bright dust cloud ours drew, his didn't) unpacked into
**three stacked ILM_visuals.tre preference-kills over one genuine port defect**:
1. **Density-ZEROED nebula datatable** (ILM copy of space_tatooine.iff; 32 nebulas load,
   zero quads created — probe-convicted: populate never ran, every density ≤ 0).
2. **His input-layout rejection** — the nebula quads feed a_vertexlit.vsh without COLOR0;
   D3D9 hands unsupplied COLOR inputs opaque WHITE and draws; his CreateInputLayout refused
   + cached the null forever. Fixed by c17 (phantom stream; the all-zeros first version drew
   invisibly — texture × black × 0 — the WHITE refinement is load-bearing).
3. **8×8-stub gas textures** (ILM copies of pt_nebulae_gas_4_*; real 349KB copies in
   patch_11_01) — capture-convicted after the draws finally issued.

Why only his client: **"same data, same overrides" was a false premise.** Both cfgs point at
the same v3.0 TRE set, but HIS mounts the raw `ILM_*.tre` archives (landmine-exposed) while
OURS mounts curated `ilm_extract` and never touches them; and stage-B-override was a
hand-picked subset, not a mirror. Also probe-methodology note: the [neb.probe] WARNING
ladder (loadScene → populate → per-nebula verdicts) split the silence in three launches.

## Cross-client shader unification (new capability)

His pipeline compiles shader sources RAW; ours preprocesses via HlslRewrite — so raw copies
of our override .vsh crashed his client (X4019: our tfcl family redeclares hemispheric
c60-63, which HIS served include already declares as `vsExtendedParallelSpecular0*`).
Resolution: **portable sources** — c19's marker + our tfcl guards alias his names when the
marker exists, declare registers otherwise. Retest: **ZERO failed compiles, ZERO FATALs**
across the full synced override set; clouds render; "space looks correct".

## Also banked this session

- c2 gl05-x64 load LIVE-VERIFIED (rasterMajor=5 → Tatooine, zero FATALs; bonus: c1 getTable
  redesign observed behaving on the D3D9 path).
- gl05 star-field reference verdict: stock = DENSE sized stars → his gl11 was faithful, OURS
  was the deviation (1px points, Plan 11-09.10 deferral) → our GS point-sprite fix, adapted
  from HIS Direct3d11_PointSprite design (a B→A adoption), live-verified.
- Gl_api load guard first live exercise (silent pass, our gl05 boot) · JTL packed-map test
  PASSED (ship component names sane).
- `e_planet_tatooine.vsh` dispositioned: stock macro-backslash bug leaves sets 1/2 as DEAD
  stray globals; renders correctly on both pipelines; his mid-row WARNING is the tombstone;
  **⚠ do NOT "fix" the backslashes** (would create real VS inputs the planet VB lacks). Our
  client logs nothing (we don't audit reflected constant alignment; his warning is the
  better behavior).

## Open board ("a lot more to do before we are completely merged" — Kenny)

- PR #1 reopen (+ body refresh for c16-c19) → Sais review → merge; whatever rework follows.
- **Corpus/data unification conversations with Sais:** his 239 conversions + asm2hlsl aren't
  in the squash repo; the portable-source convention (c19) is a candidate unification path.
  Which dataset the merged client ships; ILM mount strategy (drop raw mounts like ours vs
  ship a cleaned layer).
- **Post-merge TRE cleanup** — scope recorded in SAIS-PR-QUEUE.md (ILM same-path audit via
  tools/tre-compare → keep/restore/knob manifest; retire the loose stopgaps).
- Parked engine queue: dPVS portal fixes (blocked on cellLoaded parenting conflict #3
  conversation), c_ambient ambientBoost (corpus), wearables retry, screenshot key,
  WorldSnapshot wrong-class narrowing.
- **Our-side queued:** mirror COLOR-reads-white into OUR phantom zero buffer (Plan 11-09.8
  `getPhantomZeroBuffer` is all-zeros — same latent invisible-draw hazard c17's first
  version had) · nebula-with-bloom recheck fully closed · perf backlog (873ms NV wait, ctor
  frame) unchanged.
