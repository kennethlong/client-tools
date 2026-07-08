# 2026-07-08 — CONSULT-66 portal Signature B CLOSED (6th fix, pushed) · D3D11 peak-perf arc is the active thread

**READ FIRST after restart.** This session (2026-07-06 evening → 07-07) ran two
threads: (1) resumed the diverted **runtime TRE-read efficiency** work (part of
the D3D11 peak-perf arc) and landed the searchPath negative cache + two
cfg-parity fixes; (2) the portal see-through bug RETURNED mid-session
("Signature B") and was hunted to ground via CONSULT-66 — root-caused to a
vendored-dPVS BSP instancing defect and FIXED (the portal arc's sixth fix).
Everything is committed + pushed: `origin/master` = `dcae9526d`.

---

## 1. WHERE TO RESUME

**Primary: the D3D11 peak-performance arc** (Kenny's framing: "getting D3D11
to perform at its peak by altering D3D9 algorithms in favor of those better
suited to the D3D11 architecture"). NOTE: the 07-06 handoff's "TRE file work"
resume point meant RUNTIME TRE reading efficiency, NOT the standalone TRE
editor (Kenny corrected this explicitly; memory
`project_d3d11_peak_perf_arc_tre_runtime_reads` records it).

**Next concrete item: the constant-buffer NO_OVERWRITE ring** — the last
structural per-draw D3D9-ism in the gl11 hot path:
- `Direct3d11_ConstantBuffer.cpp` updates each slot via
  `Map(WRITE_DISCARD)` on a single shared `ID3D11Buffer` per slot →
  a driver-side buffer rename on essentially EVERY draw (VS slot 0 carries
  the per-object world matrix). This is the same churn class whose VB-ring
  variant CONSULT-58 already fixed (19x hitch collapse).
- Design: one large dynamic buffer per stage; `Map(NO_OVERWRITE)` appends
  per draw; one DISCARD per frame on wrap; bind via
  `VSSetConstantBuffers1`/`PSSetConstantBuffers1` with 256-byte-aligned
  offsets. Needs a D3D11.1 feature probe
  (`D3D11_FEATURE_D3D11_OPTIONS.ConstantBufferOffsetting`); keep the current
  per-slot path as probe-fail/config fallback. Slot payloads pad to 256B
  multiples (kMaxCBufferBytes=1152 → 1280).
- **Sizing data already measured** (sick-session census, in synthesis):
  vsB0 median 39 updates/frame, p99 51, max 54. CONSULT-58 census counters
  (`ms_frameVsUpdates`/`ms_framePsUpdates`) are already wired in
  Direct3d11_ConstantBuffer for verification deltas.
- Consider stacking a redundant-update skip (hash-compare payload before
  Map) if census shows PS slots re-pushing identical data.
- Verify: census delta + RenderDoc; watch the P19_CBUF_ZERO_TAIL semantics
  (full-buffer memset per update — the ring writes must keep the
  zero-tail guarantee or upload full padded payloads).

**Secondary (in flight): Kenny is soaking the Win32 (32-bit) client** for the
portal fix — he saw Signature B there before the Win32 rebuild carried the
fix. If he reports a sighting POST-fix, see §3 regression signatures. If the
soak is clean, the portal arc is closed at six fixes / four defects.

Runners-up (backlog, in priority order):
- Zone-in `GroundScene::init` residual ~600ms real (known class, dumps in
  logs-archive).
- Charselect avatar 371ms first-draw (one specimen).
- First-visit texture pre-warm (interior cold hitch).
- Probe strip: ALL portal probes + diag keys in one cleanup pass AFTER the
  soak (grep `PortalCullProbe`, `CONSULT-6`, `STUCK0`, `CELLSTATE`,
  `KILLDETAIL`; cfg keys: portalCullProbe, stallWatchdog*, audioDiagLog,
  censusLog).

## 2. WHAT LANDED THIS SESSION (all pushed)

**`dde0c2a0a` — perf(sharedfile): searchPath negative-lookup cache**
(CONSULT-59 deferred item, "next dominant load cost" per CONSULT-60):
- Every `TreeFile::open` paid ~3 CreateFileA kernel-miss round-trips probing
  the loose searchPath dirs (stage/override 10 / install root 9 /
  ilm_extract 5) before reaching the TOC that has the file.
- Fix: per-`TreeFile::SearchPath` hash set of fixed-up names that missed on
  disk; repeat probes answered from the set. MISSES ONLY → override-wins
  semantics unchanged. Leaf mutex (CONSULT-55 SearchCache pattern).
  Gate: `[SharedFile] searchPathNegativeCache`, default ON.
- Caveat: a loose file dropped into stage/override MID-SESSION stays
  invisible for names already probed → restart the client or set the key
  false while iterating on override files.
- Verified: both platforms green, boot-gated, Kenny live-verified.

**`dcae9526d` — fix(portal): Signature B closed (CONSULT-66)** — see §3.

**Cfg-only (gitignored, ALREADY APPLIED to stage-x64/client.cfg):**
- `[SharedFile] asynchronousLoaderCallbackTimeBudgetMs=6` — was MISSING from
  the x64 cfg (only Win32 had it since 07-04). Unbudgeted async callback
  drains were the "cantina inner-portal crossing hitch" on x64. Adding the
  key fixed it — Kenny verified "buttery smooth".
- `[Direct3d11]` section added (was entirely MISSING on x64):
  `preventDriverInternalThreading=false` (CONSULT-58 payoff soak — NV driver
  threading re-enabled; revert to true if an nvwgf2um crash appears, mdmp
  works) + `censusLog=true` (per-frame CSV → stage-x64/gl11-census.csv,
  feeds the cbuffer-ring sizing).
- `stallWatchdogMaxDumps` 6→12 (zone-in burns ~6 before anything
  interesting).

**⚠️ CFG-PARITY LESSON (the meta-find):** `stage/client.cfg` and
`stage-x64/client.cfg` are maintained by hand in duplicate and DIVERGE
SILENTLY. Two separate perf regressions this session were "the key was only
in the other platform's cfg". When a perf/behavior class is
one-platform-only: **diff the two staged cfgs FIRST, before blaming code.**

## 3. THE PORTAL SIGNATURE-B ARC (CONSULT-66) — CLOSED

Full record: `.planning/research/CONSULT-66-SYNTHESIS.md` (verdict state,
kill list, probe conviction tables, field data) + evidence pack
`CONSULT-66-portal-sigB-return-EVIDENCE.md` (3 locked field addenda) +
`CONSULT-66-raw-trace.txt`. Conviction session log archived at
`stage-x64/logs-archive/2026-07-07-consult66-CONVICTION/`.

**Defect (the arc's 6th fix, 4th distinct defect):** vendored dPVS
`Database::splitInstance` (dpvsDatabase.cpp) refines STATIC objects'
child-node assignment with an exact triangle-vs-AABB test that is
zero-epsilon against box faces. A portal is a zero-thickness quad that
routinely lies EXACTLY on a region boundary plane → the test coin-flips →
the child holding most of the portal silently LOSES its instance. The portal
survives only in a sliver leaf whose region-clamped tight bounds EXCLUDE it
(field capture: portal y -0.19..4.39 in a node testing y 4.34..165.5 — a
phantom box above the room) → `traverseNode` VF-culls that node at ordinary
poses → portal never enumerated (`tested:0`, ALL reject counters zero) →
everything beyond it = skybox. Walkable-but-invisible (collision never
consults dPVS; crossing made the player invisible to the chase camera).

**Why it was maddening:** per-SESSION lottery (BSP split placement depends on
streaming-time object population — cold disk cache shifts it; repro ranged
1-in-7 to 1-in-12 relogs), then DETERMINISTIC camera-eye-position window
within a session (chase-cam "look" orbits translate the eye — every clearing
action was an eye translation). Cantina portal nodes carried world-scale
bounds too (y up to 2891) but those CONTAIN the camera → always pass VF → no
visible hole there; only the above-the-head phantom box failed.

**Fix:** portals never take the exact-mesh refinement —
`if (ob->isStatic() && !ob->isPortal())` — keeping the conservative
box-based child mask (guaranteed instance coverage of every child the box
overlaps). Same defect family as the CONSULT-65 backface epsilon
(zero-epsilon boundary math × flat portals).

**Verification:** x64 multi-run soak clean post-fix. Win32 rebuilt + staged
2026-07-07 (~19:06), Kenny soaking now.

**Regression signatures (if a hole EVER reappears):**
- `CELLSTATE` line where a portal's `node=` box does not contain its own
  `box=` while `nNodes1` → the instancing defect is back (or a new flavor).
- `STUCK0` with `pNodeVF`/`pObVF`/`pObXVF`/`pNodeSkip` ticking while facing
  a portal + `KILLDETAIL` (failing test's box/mask/plane equations) names
  the guilty stage directly.
- All portals `c1 e0 db0` in a STUCK0 → genuinely sealed cell → the
  real-door trigger brittleness residual instead (still open, gameplay
  polish).

**Probe suite (KEPT ARMED per Kenny, all gated `[ClientGraphics]
portalCullProbe=true`, armed in BOTH staged cfgs):**
- `CELLSTATE` — on entering an interior cell: per-portal {isClosed(c),
  dpvsEnabled(e), inDatabase(db)} + portal cell-space box + owning-node test
  bounds + leaf/dirty/nInst/nNodes. Detects a broken session AT WALK-IN.
- `STUCK0`/`CLEAR0` — 1Hz heartbeat while an interior query tests zero
  portals + healing-pose edge line. Includes portal-kill counters, dPVS
  statistic deltas, aabb0, posC vs dpvsPosC (camera transform desync check).
- `KILLDETAIL` — exact operands of the failing test at portal-attributed
  kill sites (site tag, tested box, clip mask, active plane equations).
- dpvs side: `g_swgDpvsPortalRejects` now [14] ([10..13] = portal kills at
  object-AABB / exact-OBB / node-VF / node-skip-occlusion) + exports
  `swgDpvsObjectInDatabase`, `swgDpvsGetObjectCellSpaceAABB`,
  `swgDpvsGetObjectNodeInfo`, `swgDpvsGetLastPortalKillString`.
- Note: STUCK0 fires legitimately when facing a wall in a one-portal cell
  (tested:0 is legal there) — episodes <1s never log; volume is fine.

**Method note for future hunts:** the round-3 move that broke the case was
making the probe detect the BROKEN STATE (CELLSTATE at walk-in) instead of
the visual symptom — turning a 1-in-12 repro lottery into per-login state
diffs. Also: Kenny's live observations (clears-on-any-eye-translation,
walk-through-then-invisible) killed two consultant-favored hypothesis
classes before any code was read. The 4-consultant round + Fable adversarial
synthesis records are in `.planning/research/CONSULT-66-*` (the `.out` files
stay untracked per convention).

## 4. BUILD/STAGE STATE

- `master` == `origin/master` == `dcae9526d` (2 commits this session:
  negative cache `dde0c2a0a`, portal fix + probes `dcae9526d`).
- **x64**: exe + statically-linked dpvs staged (stage-x64), boot-gated.
- **Win32**: exe staged + **dpvs.dll HAND-STAGED** (stage/dpvs.dll,
  2026-07-07 19:05 — remember: NO postbuild for it; after ANY dpvs rebuild:
  `Copy-Item src\compile\win32\dpvs\Release\dpvs.dll stage\dpvs.dll`).
- Only Release configs built this session (x64 several times, Win32 once,
  serially). Debug configs are stale; canonical 5-target on both platforms
  due at next natural close-out.
- Logs: pre-fix sessions archived under `stage-x64/logs-archive/`
  (`2026-07-07-consult66-round1/` = 316 files incl. stall dumps;
  `...-CONVICTION/` = the round-3 conviction log). Live logs are fresh.
- Armed diag keys (both cfgs unless noted): portalCullProbe,
  stallWatchdogMs=100 (x64 MaxDumps=12), audioDiagLog, x64-only:
  censusLog + preventDriverInternalThreading=false soak.

## 5. GOTCHAS LEARNED / RE-CONFIRMED THIS SESSION

- **cfg parity** (§2) — diff stage/ vs stage-x64/ cfgs FIRST for
  one-platform-only behavior.
- **LNK1201 "error writing to PDB"** on rebuild = a leftover **mspdbsrv.exe**
  holding the file (killing MSBuild/cl/link is NOT enough). Kill mspdbsrv,
  delete the PDB, rebuild.
- Boot-gate tests: `Stop-Process -Name SwgClient_r` kills EVERY instance
  including Kenny's live session (allowMultipleInstances=true) — scope kills
  to the launched PID.
- Consultant output encoding varies (UTF-16LE vs UTF-8) — BOM/null-density
  check before parsing; PowerShell Select-String handles UTF-16 logs, POSIX
  grep does NOT (MSBuild logs redirected via PS `>` are UTF-16).
- Cursor consult prompts must lead with the TASK — a "read these files
  first" opener makes it summarize the files and stop.
- The negative cache means loose-file drops (stage/override) need a client
  restart (or `searchPathNegativeCache=false`) to be seen mid-iteration.
- Probe timestamps in SwgClient_report.log are UTC (local+5). Screenshot key
  timestamps correlate (that workflow paid off twice this session).

## 6. OPEN ITEMS SNAPSHOT

- Win32 portal-fix soak (Kenny, in flight) → then the arc is fully closed.
- cbuffer NO_OVERWRITE ring (§1) — the next D3D11 perf enhancement.
- preventDriverInternalThreading=false x64 soak → if extended-clean, flip
  the CODE default false and close the CONSULT-58 todo.
- Audio backlog (unchanged from 07-06 handoff §1): duck×title-fade dips,
  late-sample-start wave, endOfSample2dCallBack audit, duplicate ambients.
- Real-door trigger brittleness (gameplay polish; NOT the Signature B cause
  — that was disproven this session, the doorless portal was the victim).
- ilm-extract audit; maintainer Utinni v15 rebind (carry).
- Probe/diag strip pass after soak (one commit, grep list in §1).
