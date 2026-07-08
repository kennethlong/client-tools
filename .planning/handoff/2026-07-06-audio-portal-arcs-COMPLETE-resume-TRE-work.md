# 2026-07-06 — audio + portal arcs COMPLETE (verified, pushed) → RESUME: TRE file work

**READ FIRST after restart.** The 07-04→07-06 marathon is DONE: the audio crackle
storm (Miles version defect) and the portal see-through holes (three stacked
engine/dPVS defects) are both FIXED, live-verified by Kenny on BOTH clients, and
**pushed** (`origin/master` = `2510f1fa2`, 30 commits).

## ⏮ THE DIVERTED THREAD — resume here

**TRE file work — RUNTIME read efficiency (Kenny's 2026-07-06 correction: NOT
the standalone editor).** The thread is making TRE file reading at run time
more efficient, as part of the broader arc of getting D3D11 to perform at its
peak by replacing D3D9-era algorithms with ones suited to the D3D11
architecture. Concretely: the **TreeFile loose-searchPath stat-storm**
(CONSULT-59 deferred item, named "next dominant load cost" by CONSULT-60's
sweep) — every TreeFile::open pays ~3 CreateFileA kernel-miss round-trips
probing the loose searchPath dirs (stage/override 10 / install root 9 /
ilm_extract 5) before reaching the TOC that has the file. Fix LANDED + VERIFIED
2026-07-06 (UNCOMMITTED: ConfigSharedFile.{h,cpp} + TreeFile_SearchNode.{h,cpp}):
per-SearchPath negative-lookup cache (misses only, so override-wins semantics
unchanged; leaf mutex per the CONSULT-55 SearchCache pattern; `[SharedFile]
searchPathNegativeCache` gate, default on; caveat: loose files dropped
mid-session stay invisible for already-probed names — restart or key off).
Both platforms built green (0 unresolved), boot-gated, Kenny live-verified
"feels pretty good".

**Bonus find during verify: stage-x64/client.cfg was MISSING the CONSULT-59
`asynchronousLoaderCallbackTimeBudgetMs=6` key** (only Win32 had it) — x64
async callbacks drained unbudgeted → the cantina inner-portal crossing hitch
(NPC first-reveal burst). Added to the x64 cfg (+ stallWatchdogMaxDumps 6→12);
crossing re-verified BUTTERY SMOOTH, zero watchdog hits at the crossing.
Remaining x64 stall census: zone-in GroundScene::init ~600ms real (known
class) + benign exit-chain teardown. **Cfg-parity lesson: the two staged cfgs
diverge silently — diff them FIRST when a perf class is one-platform-only.**

Original tooling notes kept below for reference (the editor is NOT the resume
point):
- Standalone TRE editor (must be standalone/extractable — memory
  `project_tre_editor_standalone_extractable`).
- Existing assets: `tools/tre-compare/` (isolated uv package, zero engine
  imports); version-aware TRE parser at
  `D:\Code\swg-blender-plugin\swg_pipeline\tre_reader.py` (+`tre_decrypt.py`)
  — this session ALSO battle-tested it (searchTOC parsing, payload extraction
  at TOC offsets, zlib decompress; `parse_search_toc` returns a dict,
  `read_search_toc_entries(path)` → entries with `.path/.tre_file/.offset/
  .compressor/.compressed_length`).
- Constraints from memory: encryption catalog — do NOT decrypt Restoration;
  tre-compare verify fixtures are gitignored (regen on fresh checkout).
- Kenny asked explicitly that this be noted as the resume point.

## 1. AUDIO ARC — CLOSED (CONSULT-61/62/63; full record in the 07-04 checkpoint
## handoff §1-16 + .planning/research/CONSULT-63-SYNTHESIS.md)

- **Crackle storm root cause: Miles 9.3b DLL defect** (A/B-convicted: same x64
  exe, 9.3b storms, 9.3v clean). No 32-bit 9.3v exists anywhere (Restoration's
  own 32-bit ships retail 7.2a).
- **END STATE: Win32 runs vendored RETAIL 7.2a runtime**
  (`src/external/3rd/library/miles-7.2a/redist`, postbuild-staged; compile/link
  stays on the 7.2e SDK header+lib with a `MSS_MAJOR_VERSION<9` compat shim in
  Audio.cpp — room_type bus arg + premix setters RAD-stock because 7.x
  fragments are 8ms not 1ms). **x64 runs 9.3v** (9.3b header/lib). Storm GONE
  both platforms, music correct, verified across many sessions.
- The in-repo **miles-7.2e SDK redist mss32.dll is a DUD** (fails MP3 ASI
  provider discovery with ANY provider set — harness-proven, miles72test.cpp
  technique in the session record). NEVER stage it.
- **Bonus fixes en route:** 20-year `getSampleTime()` sample-handle leak
  (release was success-gated — drained all 64 driver handles on 7.x = total
  silence; fixed unconditional, `92784314b`-adjacent commit `111e74cca`);
  x64 screenshot crash (`92784314b`) = FILE* crossing into jpeg62.dll's dynamic
  UCRT — both renderers now use an in-plugin libjpeg destination manager.
- Audio follow-ups still open (backlog): load-in duck×title-fade attenuation
  dips (cosmetic, cross-version, design in 07-04 handoff §12/§14);
  late-sample-start wave (keep-alive pin + pre-warm + header-duration);
  endOfSample2dCallBack cross-thread map walk audit (Fable flag, UAF-class);
  duplicate ambient instances (5x harvester hum). Diag keys still ARMED in both
  cfgs (audioDiagLog, stallWatchdog, censusLog) — disarm in a cleanup pass.

## 2. PORTAL SEE-THROUGH ARC — CLOSED (CONSULT-64/65; synthesis =
## .planning/research/CONSULT-64-SYNTHESIS.md — read it for the full method)

**One symptom, THREE independent defects, five fixes, each convicted by
instrumented runs before writing the fix:**
1. `4dea2fdf3` — dPVS `Database::traverseNode` frustum-tested nodes against
   STALE bounds before running the dirty-node update; hoisted the update above
   the test (vendored dPVS source).
2. `7577dc9a7` — FreeChaseCamera force-copied the PLAYER's cell while the
   camera's eye lagged meters behind through doorways; the camera cell is now
   DERIVED from its own final position (player→camera walk through the portal
   graph, the positionChanged loop, passableOnly=false).
3. `458c7d386` — meshless (archway) doors never call `Portal::setClosed`:
   the flag feeds ONLY the dPVS ENABLED hook, whose sole visual purpose is
   culling behind a VISIBLE closed door mesh; archways render nothing and the
   proximity-trigger chain is brittle (doors init CLOSED at building load;
   capsule trigger misses even standing at the door) → every close was a naked
   hole. Real-meshed doors keep retail behavior; barrier/passage untouched.
4. `10821506b` — dPVS `ImpMeshModel::backFaceCull` EPSILON 0.0f → **-0.05f**
   (the Umbra author's own documented leeway knob) — kills the knife-edge where
   an eye ON a portal plane flip-flopped the cull.
5. `10821506b` — 20cm hysteresis in the camera-cell derivation (a crossing
   whose plane sits within 20cm of the eye is not taken — tag stays
   player-side; stateless).

**Verified: Kenny ran BOTH clients multiple times — no portal holes.**

Residual (low priority, filed in synthesis + todo):
- Real-door trigger brittleness (SpatialDatabase capsule misses, one-shot wake,
  DoorObject cms_keepNoAlter self-unschedule) — gameplay polish for doors with
  meshes; invisible to the player now.
- "Signature B" (tested:0 deep in-cell) never reappeared after fixes 3-5 —
  reopen via a CONSULT-66 crew round ONLY if holes return.
  **2026-07-07 UPDATE: it RETURNED and is CLOSED — CONSULT-66 (crew round +
  3 probe rounds) convicted dpvs `Database::splitInstance`: the exact
  triangle-vs-AABB refinement for static objects drops a flat portal's
  instance from a child node (zero-epsilon on box faces; per-session via
  streaming order, cold-cache repro) → portal survives only in a sliver leaf
  whose clamped tight bounds exclude it → node VF-culled → tested:0 skybox.
  FIX (6th of the arc): portals skip the exact refinement (`isStatic() &&
  !isPortal()`), keeping box-based child coverage. Fixed + verified x64
  (multi-run soak clean), fixed Win32 (dpvs.dll HAND-STAGED 2026-07-07).
  New armed probes: CELLSTATE (walk-in per-portal box/node/flags dump),
  STUCK0/CLEAR0 heartbeat, KILLDETAIL (failing-test operands), portal-
  attributed kill counters [10..13]. Full record:
  ../research/CONSULT-66-SYNTHESIS.md.**
- Camera-cell derivation boundary jitter (foyer1↔foyer2 flip at cm deltas) —
  benign with portals enabled; hysteresis covers the knife-edge.

## 3. Instrumentation still in the tree (ALL config-gated, log-on-anomaly)

`[ClientGraphics] portalCullProbe=true` armed in BOTH staged cfgs: RenderWorld
[PortalCullProbe] line (root-cell changes + count flips at camDelta<0.10, with
fwdDot + dPVS reject counters `rej=bf:ct:tt:rect:fr ph: tested: dbUpd/dbRem/
dbAdd`), DOOR open/close edge lines, DOORHIT-WAKE/FILTERED in DoorObject::hitBy,
DOORQUERY lines in SpatialDatabase (budget 400/session). The dPVS side lives in
the VENDORED SOURCE (dpvsVisibilityQuery_Traverse/_Test, dpvsDatabase,
exported `swgDpvsGetPortalRejects()`). Keep armed as a soak tripwire; strip all
probe waves in one cleanup pass later (grep `PortalCullProbe` + `CONSULT-6`).

## 4. Gotchas learned (also in memories)

- **Win32 dpvs is a DLL** — `stage/dpvs.dll` is HAND-STAGED (no postbuild!);
  after ANY dpvs rebuild: `Copy-Item src\compile\win32\dpvs\Release\dpvs.dll
  stage\dpvs.dll`. x64 links dpvs statically (exe relink picks it up).
- Win32 gl07 postbuild copy race under /m persists — retry Win32 builds
  serially (no /m) when MSB3073 hits Direct3d9_vsps.
- Always stage a FULL matched Miles set from ONE repo redist dir and
  hash-verify — mixed sets boot-AV instantly (c0000005 pre-audio-install);
  `stage-x64/_miles93b_bak` is INCOMPLETE (no binkawin64.asi), use
  `miles-9.3b/redist64` instead.
- Probe log times are UTC (local+5). Cell names repeat per building — match
  portals by POINTER. camDelta in probe lines is PER-FRAME (0.015 ≈ walking).
- Screenshot key works on both platforms now — use it to timestamp-correlate
  any sighting with the logs.

## 5. Repo state

- `master` == `origin/master` == `2510f1fa2` (30 commits pushed 2026-07-06:
  audio rounds 7-9, Miles 7.2a rollback + CONSULT-63 records, screenshot CRT
  fix, the whole portal probe evolution + 5 fixes + CONSULT-64/65 records).
- Untracked leftovers that STAY untracked: CONSULT-56/57 `.out` files.
- Only Release configs rebuilt during the marathon; a canonical 5-target build
  on BOTH platforms (incl. Debug if desired) is due at the next natural close-out.
- RenderDoc captures `stage-x64/Capture100-103.rdc` (the original conviction
  evidence) can be deleted or archived at will.
