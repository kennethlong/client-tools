# 2026-08-15 — SESSION CLOSE: Sais fix queue LANDED (15 commits), field session convictions

**READ FIRST (latest session-close).** Continues the 2026-08-14 session-close (Sais A/B arc).
This session: Sais replied → full green light → the whole fix queue built, plus a live field
session with Kenny that root-caused and fixed two of his client's most visible defects.
**The live queue tracker is [.planning/SAIS-PR-QUEUE.md](../SAIS-PR-QUEUE.md)** — per-commit
detail, lessons, parked items. This file is the narrative + state.

## State

- **Our repo:** `master` == `origin/master` == `bdb21dd77` + this close's docs commit; ALL pushed.
  Both platform build gates green (x64 + Win32 serial 5-target, 0 unresolved externals) before
  the push. `stage-x64/client.cfg` restored from `.swgsource-bak` at session start (normal dev).
- **Sais's deliverable repo:** `Galaxies-Reborn/swg-source-x64-dx11` (**we have ADMIN** — accepted
  invite this session; NOT a fork of anything, standalone squash of SWG-Source ancestry +
  `3ab047315`). Local clone at `D:/Code/Galaxies-Reborn/swg-source-x64-dx11`, remote `kenny` =
  our repo (for cross-repo cherry-picks). **Branch `strict-data-defaults`, tip `1f73947ff`,
  15 commits, pushed.** History rebased once (getTable fixup autosquashed into c1) — clean,
  zero `.planning` stowaways (our fix commits carry planning docs; STRIP on every cherry-pick).
- **PR #1 on his repo: CLOSED deliberately** (Kenny: no review pressure on Sais yet). Title +
  full body are already written and attached to it — **reopen with `gh pr reopen 1` when Kenny
  says go.** Delivery model (memory `feedback_sais_one_pr_clean_commits`): ONE PR, clean atomic
  commits, Sais reviews, we never self-merge, never push his `master`.
- **stage-B-x64** (his runnable client) carries the CURRENT branch-tip binaries (exe + gl11
  restaged repeatedly this session; gl05/06/07 from the branch too). His cfg there points at OUR
  v3.0 data; `stage-B-override` still has our fog/c_ambient/nebula loose fixes ACTIVE. The temp
  `frameRateLimit` diagnostic key was added and REMOVED — cfg is clean (BOM-verified).
- Sais's Discord message (via Kenny): "covert mode, put my OCD to good use" = full autonomy,
  no per-fix approvals; later granted repo rights. He has NOT yet been pointed at the branch.

## The 15 commits (headline; full table in SAIS-PR-QUEUE.md)

c1 strict-data-by-default + `[SharedFoundation] strictData=false` opt-out (STRICT_DATA_FATAL in
Fatal.h; ~60 sites; NetworkHandler rethrow UNCONDITIONAL) · c2 `Gl_dll.def` includes
`Production.h` (**the root of his never-loading x64 D3D9** — `#if PRODUCTION == 0` resolved
per-TU by include order, 864v896) · c3 window reveal activates (drop `SWP_NOACTIVATE`) —
**LIVE-VERIFIED** · c4-c12 our engine fixes cherry-picked (combat erase-iterator UB,
FPU_PRESERVE, SearchCache lock+zlib, CuiMediator re-entrancy + snapshot, HUD/reticle guards,
heap-corruption trio, CONSULT-56 follow-up) · c13 **WorldSnapshot delete drain restored** (his
tree had the IDENTICAL lifetime no-op — live `#if 1` walk never computes distances — AND the
refusal-after-teardown hazard; stock-ancestor defect both forks inherited; + detailLevelChanged
saveList[i]) · c14 skeletal ARGB==0→white rewrite gated behind strictData + zero-ARGB build
WARNING · c15 **DX9 draw-time alpha-fade overrides ported — LIVE-VERIFIED** (see below).

## The field session (the good stuff)

1. **"Still no focus" was a STALE-BINARY verdict** — Kenny ran the 2:21 PM pre-fix stage-B set;
   nothing auto-stages there. RESTAGE stage-B-x64 BY HAND after every branch build. On the real
   binaries the focus fix verified first launch.
2. **Boot FATAL on `datatables/minigame/mahjong/test.iff`** → my c1 getTable chokepoint FATAL
   was WRONG: `getTable(name, true)` is used BOTH as "must have" and as an optional-content
   PROBE (the mahjong layout index names files that never shipped, even in v3.0). **Redesign:
   strict = EXACT STOCK semantics (NULL return; caller-site STRICT_DATA_FATALs carry the
   strictness), lenient = sentinel, both WARN.** Folded into c1 via autosquash. Lesson: a
   fail-fast chokepoint must never FATAL on a probe pattern.
3. **The "bloom saturation aura" on every character/NPC first sight — ROOT-CAUSED AND FIXED
   (c15, live-verified: "faded in as planned, no oversaturation, looked proper").** Mechanism:
   the stock glow design uses RT ALPHA as per-pixel bloom intensity (`2d_bloom` composites
   `base + bloom*bloom.a`). DX9 therefore masks the alpha colour-write and forces blend at draw
   time while an object fades (`Direct3d9.cpp:3953-3961`); his D3D11 port plumbed the fade
   constants but never carried those two overrides — every fading character wrote its ramping
   fade opacity across its silhouette in the GLOW channel. Fix: per-pass fade-VARIANT cached
   blend state, selected **PER DRAW** in `prepareToDraw` (the engine sets fade per PRIMITIVE;
   his pass apply covers a sorted shader group — apply-time selection would leak fade across
   shader-sharing objects; DX9 chooses at draw time for the same reason). Bonus: his client
   gains actual translucent fade-ins (objects previously rendered opaque while computing an
   opacity nothing consumed).
4. **Flash-class bugs are CAPTURE-PROOF heisenbugs.** RenderDoc capture overhead AND low fps
   caps both hand the async loader wall-clock → the race resolves → no flash (measured:
   invisible at 10 fps cap, ~100 ms at 30, 200-500 ms uncapped — duration GROWS with fps).
   Convict by code-probe log + stock-contract reasoning, not capture. The `frameRateLimit`
   walk-up (10→30→uncapped) is the cheap fps-dependence probe.
5. **The skeletal white rewrite was EXONERATED for this flash by its own probe** — the c14
   WARNING logged ZERO zero-ARGB builds during a live flash. The gate stays on its merits
   (it IS a data-era compensation), but it was not this bug.

## Falsified this session (don't re-derive)

- **The A/B "horizon over-bloom" (conviction #3 of 08-14) is STOCK `2d_bloom`** —
  `pixel_program/2d_bloom.psh` in ILM_visuals.tre (`base + bloom*bloom.a`,
  instruction-identical to the captured draw 3755), driven by stock
  `PostProcessingEffectsManager`, gated by the per-machine graphics option — a SETTINGS
  difference between the two installs, NOT his conversion corpus. Kenny LIKES the look; it's
  already a knob. Retraction recorded in CONSULT-74-AB-RUN-FINDINGS.md (with the sub-retraction
  that draws 3702-3738 WERE the bloom chain). Task #5's "emissive/additive generator rule"
  scope collapsed to: c_ambient parameterization + re-check nebula saturation WITH bloom
  accounted for. NOTE: his corpus e_nebula/emissive shaders were checked — semantically sound
  (lerp-based, and e_nebula matches our premultiply semantics).
- His corpus and the asm2hlsl generator are NOT in the squash repo at all (0 .psh/.vsh) —
  flagged in the PR body as a gap he must decide on.

## Our repo adoptions (pushed, both platforms gated)

`e34760a0a..bdb21dd77`: palette-clamp diagnostic (his e961a57c5) · TextureFormatInfo 2 rows ·
**color-write-mask engine-ARGB↔D3D11-RGBA R/B swap** (his find, confirmed against our own D3D9
table; chokepoint translation in `Direct3d11_StateCache::setColorWriteEnable`) · packed-map
int32 counts (server-interop caveat in the message; JTL x64 test still open) · **transform
send-rate throttle** (`[ClientGame] transformSendRate`=60, 0=off — the movement-stall-at-
high-fps stock defect) · Archive.h short-buffer guards (guards ONLY — kept our no-8-byte-
overload compile trap; his 64-bit overloads deliberately NOT taken) · `Gl_dll.def` Production.h
(same landmine as his) · `GetGlApiStructSize` load guard (Graphics.cpp FATALs on exe/DLL Gl_api
size mismatch — first live exercise happens on Kenny's next OUR-client boot).

## Open / next

- **Kenny flips the PR live** when ready (`gh pr reopen 1`); body/title already on it.
- Kenny watch item: his client now has real fade-ins — if the look ever reads WRONG, say so
  (it's stock behavior he never had).
- Parked (reasons in SAIS-PR-QUEUE.md): dPVS portal fixes (blocked on his cellLoaded parenting
  flip — conflict #3 conversation), c_ambient ambientBoost knob (corpus, not squash),
  char-select wearables retry, dead screenshot key, WorldSnapshot wrong-class narrowing
  follow-up, #10 pile (space ambient sound both clients).
- Our-side open: JTL x64 packed-map empirical test; gl05 x64 rasterMajor=5 runtime check on HIS
  client (should now load past his Gl_api guard — c2); nebula-with-bloom-off recheck.
- AGENTS.md + CLAUDE.md are GITIGNORED in this repo (discovered this session) — the new
  "Sais / Galaxies-Reborn collaboration" section in AGENTS.md lives on disk only.
- Memory updated: `project_sais_collaboration_ab_run` (rewritten to landed-state),
  `feedback_sais_one_pr_clean_commits` (new: ONE PR at the end, clean commits, he reviews).
